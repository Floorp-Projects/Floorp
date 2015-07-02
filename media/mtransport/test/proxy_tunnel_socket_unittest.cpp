/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original authors: ekr@rtfm.com; ryan@tokbox.com

#include <iostream>

#include "nspr.h"
#include "nss.h"
#include "ssl.h"

#include "mozilla/Scoped.h"

extern "C" {
#include "nr_api.h"
#include "nr_socket.h"
#include "nr_proxy_tunnel.h"
#include "transport_addr.h"
#include "stun.h"
}

#include "databuffer.h"
#include "mtransport_test_utils.h"
#include "dummysocket.h"

#include "nr_socket_prsock.h"
#include "nriceresolverfake.h"

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"
#include "gtest_utils.h"

using namespace mozilla;
MtransportTestUtils *test_utils;

const std::string kRemoteAddr = "192.0.2.133";
const uint16_t kRemotePort = 3333;

const std::string kProxyHost = "example.com";
const std::string kProxyAddr = "192.0.2.134";
const uint16_t kProxyPort = 9999;

const std::string kHelloMessage = "HELLO";
const std::string kGarbageMessage = "xxxxxxxxxx";

std::string connect_message(const std::string &host, uint16_t port, const std::string &tail = "") {
  std::stringstream ss;
  ss << "CONNECT " << host << ":" << port << " HTTP/1.0\r\n\r\n" << tail;
  return ss.str();
}

std::string connect_response(int code, const std::string &tail = "") {
  std::stringstream ss;
  ss << "HTTP/1.0 " << code << "\r\n\r\n" << tail;
  return ss.str();
}

class DummyResolver {
 public:
  DummyResolver() {
    vtbl_ = new nr_resolver_vtbl;
    vtbl_->destroy = &DummyResolver::destroy;
    vtbl_->resolve = &DummyResolver::resolve;
    vtbl_->cancel = &DummyResolver::cancel;
    nr_resolver_create_int((void *)this, vtbl_, &resolver_);
  }

  ~DummyResolver() {
    nr_resolver_destroy(&resolver_);
    delete vtbl_;
  }

  static int destroy(void **objp) {
    return 0;
  }

  static int resolve(void *obj,
                     nr_resolver_resource *resource,
                     int (*cb)(void *cb_arg, nr_transport_addr *addr),
                     void *cb_arg,
                     void **handle) {
    nr_transport_addr addr;

    nr_str_port_to_transport_addr(
        (char *)kProxyAddr.c_str(), kProxyPort, IPPROTO_TCP, &addr);

    cb(cb_arg, &addr);
    return 0;
  }

  static int cancel(void *obj, void *handle) {
    return 0;
  }

  nr_resolver *get_nr_resolver() {
    return resolver_;
  }

 private:
  nr_resolver_vtbl *vtbl_;
  nr_resolver *resolver_;
};

class ProxyTunnelSocketTest : public ::testing::Test {
 public:
  ProxyTunnelSocketTest()
      : socket_impl_(nullptr),
        nr_socket_(nullptr) {}

  ~ProxyTunnelSocketTest() {
    nr_socket_destroy(&nr_socket_);
    nr_proxy_tunnel_config_destroy(&config_);
  }

  void SetUp() {
    nsRefPtr<DummySocket> dummy(new DummySocket());

    nr_resolver_ = resolver_impl_.get_nr_resolver();

    int r = nr_str_port_to_transport_addr(
        (char *)kRemoteAddr.c_str(), kRemotePort, IPPROTO_TCP, &remote_addr_);
    ASSERT_EQ(0, r);

    r = nr_str_port_to_transport_addr(
        (char *)kProxyAddr.c_str(), kProxyPort, IPPROTO_TCP, &proxy_addr_);
    ASSERT_EQ(0, r);

    nr_proxy_tunnel_config_create(&config_);
    nr_proxy_tunnel_config_set_resolver(config_, nr_resolver_);
    nr_proxy_tunnel_config_set_proxy(config_, kProxyAddr.c_str(), kProxyPort);

    r = nr_socket_proxy_tunnel_create(
        config_,
        dummy->get_nr_socket(),
        &nr_socket_);
    ASSERT_EQ(0, r);

    socket_impl_ = dummy.forget();  // Now owned by nr_socket_.
  }

  nr_socket *socket() { return nr_socket_; }

 protected:
  nsRefPtr<DummySocket> socket_impl_;
  DummyResolver resolver_impl_;
  nr_socket *nr_socket_;
  nr_resolver *nr_resolver_;
  nr_proxy_tunnel_config *config_;
  nr_transport_addr proxy_addr_;
  nr_transport_addr remote_addr_;
};


TEST_F(ProxyTunnelSocketTest, TestCreate) {
}

TEST_F(ProxyTunnelSocketTest, TestConnectProxyAddress) {
  int r = nr_socket_connect(nr_socket_, &remote_addr_);
  ASSERT_EQ(0, r);

  ASSERT_EQ(0, nr_transport_addr_cmp(socket_impl_->get_connect_addr(), &proxy_addr_,
        NR_TRANSPORT_ADDR_CMP_MODE_ALL));
}

TEST_F(ProxyTunnelSocketTest, TestConnectProxyRequest) {
  int r = nr_socket_connect(nr_socket_, &remote_addr_);
  ASSERT_EQ(0, r);

  size_t written = 0;
  r = nr_socket_write(nr_socket_, kHelloMessage.c_str(), kHelloMessage.size(), &written, 0);
  ASSERT_EQ(0, r);

  std::string msg = connect_message(kRemoteAddr, kRemotePort, kHelloMessage);
  socket_impl_->CheckWriteBuffer(reinterpret_cast<const uint8_t *>(msg.c_str()), msg.size());
}

TEST_F(ProxyTunnelSocketTest, TestConnectProxyHostRequest) {
  int r = nr_socket_destroy(&nr_socket_);
  ASSERT_EQ(0, r);

  nsRefPtr<DummySocket> dummy(new DummySocket());

  nr_proxy_tunnel_config_set_proxy(config_, kProxyHost.c_str(), kProxyPort);

  r = nr_socket_proxy_tunnel_create(
      config_,
      dummy->get_nr_socket(),
      &nr_socket_);
  ASSERT_EQ(0, r);

  socket_impl_ = dummy.forget();  // Now owned by nr_socket_.

  r = nr_socket_connect(nr_socket_, &remote_addr_);
  ASSERT_EQ(R_WOULDBLOCK, r);

  size_t written = 0;
  r = nr_socket_write(nr_socket_, kHelloMessage.c_str(), kHelloMessage.size(), &written, 0);
  ASSERT_EQ(0, r);

  std::string msg = connect_message(kRemoteAddr, kRemotePort, kHelloMessage);
  socket_impl_->CheckWriteBuffer(reinterpret_cast<const uint8_t *>(msg.c_str()), msg.size());
}

TEST_F(ProxyTunnelSocketTest, TestConnectProxyWrite) {
  int r = nr_socket_connect(nr_socket_, &remote_addr_);
  ASSERT_EQ(0, r);

  size_t written = 0;
  r = nr_socket_write(nr_socket_, kHelloMessage.c_str(), kHelloMessage.size(), &written, 0);
  ASSERT_EQ(0, r);

  socket_impl_->ClearWriteBuffer();

  r = nr_socket_write(nr_socket_, kHelloMessage.c_str(), kHelloMessage.size(), &written, 0);
  ASSERT_EQ(0, r);

  socket_impl_->CheckWriteBuffer(reinterpret_cast<const uint8_t *>(kHelloMessage.c_str()),
      kHelloMessage.size());
}

TEST_F(ProxyTunnelSocketTest, TestConnectProxySuccessResponse) {
  int r = nr_socket_connect(nr_socket_, &remote_addr_);
  ASSERT_EQ(0, r);

  std::string resp = connect_response(200, kHelloMessage);
  socket_impl_->SetReadBuffer(reinterpret_cast<const uint8_t *>(resp.c_str()), resp.size());

  char buf[4096];
  size_t read = 0;
  r = nr_socket_read(nr_socket_, buf, sizeof(buf), &read, 0);
  ASSERT_EQ(0, r);

  ASSERT_EQ(kHelloMessage.size(), read);
  ASSERT_EQ(0, memcmp(buf, kHelloMessage.c_str(), kHelloMessage.size()));
}

TEST_F(ProxyTunnelSocketTest, TestConnectProxyRead) {
  int r = nr_socket_connect(nr_socket_, &remote_addr_);
  ASSERT_EQ(0, r);

  std::string resp = connect_response(200, kHelloMessage);
  socket_impl_->SetReadBuffer(reinterpret_cast<const uint8_t *>(resp.c_str()), resp.size());

  char buf[4096];
  size_t read = 0;
  r = nr_socket_read(nr_socket_, buf, sizeof(buf), &read, 0);
  ASSERT_EQ(0, r);

  socket_impl_->ClearReadBuffer();
  socket_impl_->SetReadBuffer(reinterpret_cast<const uint8_t *>(kHelloMessage.c_str()),
      kHelloMessage.size());

  r = nr_socket_read(nr_socket_, buf, sizeof(buf), &read, 0);
  ASSERT_EQ(0, r);

  ASSERT_EQ(kHelloMessage.size(), read);
  ASSERT_EQ(0, memcmp(buf, kHelloMessage.c_str(), kHelloMessage.size()));
}

TEST_F(ProxyTunnelSocketTest, TestConnectProxyReadNone) {
  int r = nr_socket_connect(nr_socket_, &remote_addr_);
  ASSERT_EQ(0, r);

  std::string resp = connect_response(200);
  socket_impl_->SetReadBuffer(reinterpret_cast<const uint8_t *>(resp.c_str()), resp.size());

  char buf[4096];
  size_t read = 0;
  r = nr_socket_read(nr_socket_, buf, sizeof(buf), &read, 0);
  ASSERT_EQ(R_WOULDBLOCK, r);

  socket_impl_->ClearReadBuffer();
  socket_impl_->SetReadBuffer(reinterpret_cast<const uint8_t *>(kHelloMessage.c_str()),
      kHelloMessage.size());

  r = nr_socket_read(nr_socket_, buf, sizeof(buf), &read, 0);
  ASSERT_EQ(0, r);
}

TEST_F(ProxyTunnelSocketTest, TestConnectProxyFailResponse) {
  int r = nr_socket_connect(nr_socket_, &remote_addr_);
  ASSERT_EQ(0, r);

  std::string resp = connect_response(500, kHelloMessage);
  socket_impl_->SetReadBuffer(reinterpret_cast<const uint8_t *>(resp.c_str()), resp.size());

  char buf[4096];
  size_t read = 0;
  r = nr_socket_read(nr_socket_, buf, sizeof(buf), &read, 0);
  ASSERT_NE(0, r);
}

TEST_F(ProxyTunnelSocketTest, TestConnectProxyGarbageResponse) {
  int r = nr_socket_connect(nr_socket_, &remote_addr_);
  ASSERT_EQ(0, r);

  socket_impl_->SetReadBuffer(reinterpret_cast<const uint8_t *>(kGarbageMessage.c_str()),
      kGarbageMessage.size());

  char buf[4096];
  size_t read = 0;
  r = nr_socket_read(nr_socket_, buf, sizeof(buf), &read, 0);
  ASSERT_EQ(0ul, read);
}

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
