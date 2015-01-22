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

static UniquePtr<DataBuffer> merge(UniquePtr<DataBuffer> a, UniquePtr<DataBuffer> b) {
  if (a && a->len() && b && b->len()) {
    UniquePtr<DataBuffer> merged(new DataBuffer());
    merged->Allocate(a->len() + b->len());

    memcpy(merged->data(), a->data(), a->len());
    memcpy(merged->data() + a->len(), b->data(), b->len());

    return merged;
  }

  if (a && a->len()) {
    return a;
  }

  if (b && b->len()) {
    return b;
  }

  return nullptr;
}

class DummySocket : public NrSocketBase {
 public:
  DummySocket()
      : writable_(UINT_MAX),
        write_buffer_(nullptr),
        readable_(UINT_MAX),
        read_buffer_(nullptr),
        cb_(nullptr),
        cb_arg_(nullptr),
        self_(nullptr) {}

  // the nr_socket APIs
  virtual int create(nr_transport_addr *addr) {
    return 0;
  }

  virtual int sendto(const void *msg, size_t len,
                     int flags, nr_transport_addr *to) {
    MOZ_CRASH();
    return 0;
  }

  virtual int recvfrom(void * buf, size_t maxlen,
                       size_t *len, int flags,
                       nr_transport_addr *from) {
    MOZ_CRASH();
    return 0;
  }

  virtual int getaddr(nr_transport_addr *addrp) {
    MOZ_CRASH();
    return 0;
  }

  virtual void close() {
  }

  virtual int connect(nr_transport_addr *addr) {
    nr_transport_addr_copy(&connect_addr_, addr);
    return 0;
  }

  virtual int write(const void *msg, size_t len, size_t *written) {
    size_t to_write = std::min(len, writable_);

    if (to_write) {
      UniquePtr<DataBuffer> msgbuf(new DataBuffer(static_cast<const uint8_t *>(msg), to_write));
      write_buffer_ = merge(Move(write_buffer_), Move(msgbuf));
    }

    *written = to_write;

    return 0;
  }

  virtual int read(void* buf, size_t maxlen, size_t *len) {
    if (!read_buffer_.get()) {
      return R_WOULDBLOCK;
    }

    size_t to_read = std::min(read_buffer_->len(),
                              std::min(maxlen, readable_));

    memcpy(buf, read_buffer_->data(), to_read);
    *len = to_read;

    if (to_read < read_buffer_->len()) {
      read_buffer_.reset(new DataBuffer(read_buffer_->data() + to_read,
                                    read_buffer_->len() - to_read));
    } else {
      read_buffer_.reset();
    }

    return 0;
  }

  // Implementations of the async_event APIs.
  // These are no-ops because we handle scheduling manually
  // for test purposes.
  virtual int async_wait(int how, NR_async_cb cb, void *cb_arg,
                         char *function, int line) {
    EXPECT_EQ(nullptr, cb_);
    cb_ = cb;
    cb_arg_ = cb_arg;

    return 0;
  }

  virtual int cancel(int how) {
    cb_ = nullptr;
    cb_arg_ = nullptr;

    return 0;
  }


  // Read/Manipulate the current state.
  void CheckWriteBuffer(const uint8_t *data, size_t len) {
    if (!len) {
      EXPECT_EQ(nullptr, write_buffer_.get());
    } else {
      EXPECT_NE(nullptr, write_buffer_.get());
      ASSERT_EQ(len, write_buffer_->len());
      ASSERT_EQ(0, memcmp(data, write_buffer_->data(), len));
    }
  }

  void ClearWriteBuffer() {
    write_buffer_.reset();
  }

  void SetWritable(size_t val) {
    writable_ = val;
  }

  void FireWritableCb() {
    NR_async_cb cb = cb_;
    void *cb_arg = cb_arg_;

    cb_ = nullptr;
    cb_arg_ = nullptr;

    cb(this, NR_ASYNC_WAIT_WRITE, cb_arg);
  }

  void SetReadBuffer(const uint8_t *data, size_t len) {
    EXPECT_EQ(nullptr, write_buffer_.get());
    read_buffer_.reset(new DataBuffer(data, len));
  }

  void ClearReadBuffer() {
    read_buffer_.reset();
  }

  void SetReadable(size_t val) {
    readable_ = val;
  }

  nr_socket *get_nr_socket() {
    if (!self_) {
      int r = nr_socket_create_int(this, vtbl(), &self_);
      AddRef();
      if (r)
        return nullptr;
    }

    return self_;
  }

  nr_transport_addr *get_connect_addr() {
    return &connect_addr_;
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DummySocket);

 private:
  ~DummySocket() {}

  DISALLOW_COPY_ASSIGN(DummySocket);

  size_t writable_;  // Amount we allow someone to write.
  UniquePtr<DataBuffer> write_buffer_;
  size_t readable_;   // Amount we allow someone to read.
  UniquePtr<DataBuffer> read_buffer_;

  NR_async_cb cb_;
  void *cb_arg_;
  nr_socket *self_;

  nr_transport_addr connect_addr_;
};

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

    nr_ip4_str_port_to_transport_addr(
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

    int r = nr_ip4_str_port_to_transport_addr(
        (char *)kRemoteAddr.c_str(), kRemotePort, IPPROTO_TCP, &remote_addr_);
    ASSERT_EQ(0, r);

    r = nr_ip4_str_port_to_transport_addr(
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
