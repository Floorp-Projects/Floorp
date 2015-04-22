/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <iostream>
#include <vector>

#include "mozilla/Scoped.h"
#include "mozilla/Atomics.h"
#include "runnable_utils.h"
#include "nss.h"
#include "pk11pub.h"

extern "C" {
#include "nr_api.h"
#include "nr_socket.h"
#include "transport_addr.h"
#include "ice_ctx.h"
#include "nr_socket_multi_tcp.h"
}

#include "mtransport_test_utils.h"
#include "gtest_ringbuffer_dumper.h"

#include "nr_socket_prsock.h"

#include "nricectx.h"
#include "nricemediastream.h"

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"
#include "gtest_utils.h"

using namespace mozilla;
MtransportTestUtils *test_utils;

namespace {

class MultiTcpSocketTest : public ::testing::Test {
 public:
  MultiTcpSocketTest()
    :socks(3,nullptr),
     readable(false),
     ice_ctx_(NrIceCtx::Create("stun", true))
   {}

  ~MultiTcpSocketTest() {
    test_utils->sts_target()->Dispatch(
            WrapRunnable(
                this, &MultiTcpSocketTest::Shutdown_s),
            NS_DISPATCH_SYNC);
  }

  DISALLOW_COPY_ASSIGN(MultiTcpSocketTest);

  static void SockReadable(NR_SOCKET s, int how, void *arg) {
    MultiTcpSocketTest *obj=static_cast<MultiTcpSocketTest *>(arg);
    obj->SetReadable(true);
  }

  void Shutdown_s() {
    ice_ctx_ = nullptr;
    for (std::vector<nr_socket *>::iterator it=socks.begin();
         it!=socks.end(); ++it) {
      nr_socket_destroy(&(*it));
    }
  }

  static uint16_t GetRandomPort() {
    uint16_t result;
    if (PK11_GenerateRandom((unsigned char*)&result, 2) != SECSuccess) {
      MOZ_ASSERT(false);
      return 0;
    }
    return result;
  }

  static uint16_t EnsureEphemeral(uint16_t port) {
    // IANA ephemeral port range (49152 to 65535)
    return port | 49152;
  }

  void Create_s(nr_socket_tcp_type tcp_type, nr_socket *stun_server_socket,
                int use_framing, nr_socket **sock) {
    nr_transport_addr local;
    // Get start of port range for test
    static unsigned short port_s = GetRandomPort();
    int r;

    if (stun_server_socket) {
      nr_transport_addr stun_addr;
      int port;
      char stun_host[1000];

      r = nr_socket_getaddr(stun_server_socket, &stun_addr);
      ASSERT_EQ(0, r);
      r = nr_transport_addr_get_port(&stun_addr, &port);
      ASSERT_EQ(0, r);
      r = nr_transport_addr_get_addrstring(&stun_addr, &stun_host[0],
                                           sizeof(stun_host));
      ASSERT_EQ(0, r);

      std::vector<NrIceStunServer> stun_servers;
      ScopedDeletePtr<NrIceStunServer> server(NrIceStunServer::Create(
          stun_host, port, kNrIceTransportTcp));
      stun_servers.push_back(*server);

      ASSERT_TRUE(NS_SUCCEEDED(ice_ctx_->SetStunServers(stun_servers)));
    }

    r = 1;
    for (int tries=10; tries && r; --tries) {
      r = nr_str_port_to_transport_addr(
        (char *)"127.0.0.1", EnsureEphemeral(port_s++), IPPROTO_TCP, &local);
      ASSERT_EQ(0, r);

      r = nr_socket_multi_tcp_create(ice_ctx_->ctx(),
          &local, tcp_type, 1, use_framing, 2048, sock);
    }

    ASSERT_EQ(0, r);
    printf("Creating socket on %s\n", local.as_string);
    r = nr_socket_multi_tcp_set_readable_cb(*sock,
        &MultiTcpSocketTest::SockReadable, this);
    ASSERT_EQ(0, r);
  }

  nr_socket *Create(nr_socket_tcp_type tcp_type,
                    nr_socket *stun_server_socket = NULL,
                    int use_framing = 1) {
    nr_socket *sock=nullptr;
    test_utils->sts_target()->Dispatch(
            WrapRunnable(
                this, &MultiTcpSocketTest::Create_s, tcp_type,
                stun_server_socket, use_framing, &sock),
            NS_DISPATCH_SYNC);
    return sock;
  }

  void Listen_s(nr_socket *sock) {
    nr_transport_addr addr;
    int r=nr_socket_getaddr(sock, &addr);
    ASSERT_EQ(0, r);
    printf("Listening on %s\n", addr.as_string);
    r = nr_socket_listen(sock, 5);
    ASSERT_EQ(0, r);
  }

  void Listen(nr_socket *sock) {
    test_utils->sts_target()->Dispatch(
            WrapRunnable(
                this, &MultiTcpSocketTest::Listen_s, sock),
            NS_DISPATCH_SYNC);
  }

  void Connect_s(nr_socket *from, nr_socket *to) {
    nr_transport_addr addr_to;
    nr_transport_addr addr_from;
    int r=nr_socket_getaddr(to, &addr_to);
    ASSERT_EQ(0, r);
    r=nr_socket_getaddr(from, &addr_from);
    ASSERT_EQ(0, r);
    printf("Connecting from %s to %s\n", addr_from.as_string, addr_to.as_string);
    r=nr_socket_connect(from, &addr_to);
    ASSERT_EQ(0, r);
  }

  void Connect(nr_socket *from, nr_socket *to) {
    test_utils->sts_target()->Dispatch(
            WrapRunnable(
                this, &MultiTcpSocketTest::Connect_s, from, to),
            NS_DISPATCH_SYNC);
  }

  void ConnectSo_s(nr_socket *so1, nr_socket *so2) {
    nr_transport_addr addr_so1;
    nr_transport_addr addr_so2;
    int r=nr_socket_getaddr(so1, &addr_so1);
    ASSERT_EQ(0, r);
    r=nr_socket_getaddr(so2, &addr_so2);
    ASSERT_EQ(0, r);
    printf("Connecting SO %s <-> %s\n", addr_so1.as_string, addr_so2.as_string);
    r=nr_socket_connect(so1, &addr_so2);
    ASSERT_EQ(0, r);
    r=nr_socket_connect(so2, &addr_so1);
    ASSERT_EQ(0, r);
  }

  void ConnectSo(nr_socket *from, nr_socket *to) {
    test_utils->sts_target()->Dispatch(
            WrapRunnable(
                this, &MultiTcpSocketTest::ConnectSo_s, from, to),
            NS_DISPATCH_SYNC);
  }

  void SendData_s(nr_socket *from, nr_socket *to, const char *data,
                  size_t len) {
    nr_transport_addr addr_from, addr_to;

    int r=nr_socket_getaddr(to, &addr_to);
    ASSERT_EQ(0, r);
    r=nr_socket_getaddr(from, &addr_from);
    ASSERT_EQ(0, r);
    printf("Sending %lu bytes %s -> %s\n", (unsigned long)len,
      addr_from.as_string, addr_to.as_string);
    r=nr_socket_sendto(from, data, len, 0, &addr_to);
    ASSERT_EQ(0, r);
  }

  void SendData(nr_socket *from, nr_socket *to, const char *data, size_t len) {
    test_utils->sts_target()->Dispatch(
            WrapRunnable(
                this, &MultiTcpSocketTest::SendData_s, from, to, data, len),
            NS_DISPATCH_SYNC);
  }

  void RecvData_s(nr_socket *expected_from, nr_socket *sent_to,
                  const char *expected_data, size_t expected_len) {
    SetReadable(false);
    char received_data[expected_len+1];
    nr_transport_addr addr_from, addr_to;
    nr_transport_addr retaddr;
    size_t retlen;

    int r=nr_socket_getaddr(sent_to, &addr_to);
    ASSERT_EQ(0, r);
    r=nr_socket_getaddr(expected_from, &addr_from);
    ASSERT_EQ(0, r);
    printf("Receiving %lu bytes %s <- %s\n", (unsigned long)expected_len,
      addr_to.as_string, addr_from.as_string);
    r=nr_socket_recvfrom(sent_to, received_data, expected_len+1,
                         &retlen, 0, &retaddr);
    ASSERT_EQ(0, r);
    r=nr_transport_addr_cmp(&retaddr, &addr_from,
                            NR_TRANSPORT_ADDR_CMP_MODE_ALL);
    ASSERT_EQ(0, r);
    ASSERT_EQ(expected_len, retlen);
    r=memcmp(expected_data, received_data, retlen);
    ASSERT_EQ(0, r);
  }

  void RecvData(nr_socket *expected_from, nr_socket *sent_to,
                const char *expected_data, size_t expected_len) {
    ASSERT_TRUE_WAIT(IsReadable(), 1000);
    test_utils->sts_target()->Dispatch(
            WrapRunnable(
                this, &MultiTcpSocketTest::RecvData_s, expected_from, sent_to,
                expected_data, expected_len),
            NS_DISPATCH_SYNC);
  }

  void TransferData(nr_socket *from, nr_socket *to, const char *data,
                    size_t len) {
    SendData(from, to, data, len);
    RecvData(from, to, data, len);
  }

 protected:
  bool IsReadable() const {
    return readable;
  }
  void SetReadable(bool r) {
    readable=r;
  }
  std::vector<nr_socket *> socks;
  Atomic<bool> readable;
  nsRefPtr<NrIceCtx> ice_ctx_;
};
}

TEST_F(MultiTcpSocketTest, TestListen) {
  socks[0] = Create(TCP_TYPE_PASSIVE);
  Listen(socks[0]);
}

TEST_F(MultiTcpSocketTest, TestConnect) {
  socks[0] = Create(TCP_TYPE_PASSIVE);
  socks[1] = Create(TCP_TYPE_ACTIVE);
  socks[2] = Create(TCP_TYPE_ACTIVE);
  Listen(socks[0]);
  Connect(socks[1], socks[0]);
  Connect(socks[2], socks[0]);
}

TEST_F(MultiTcpSocketTest, TestTransmit) {
  const char data[] = "TestTransmit";
  socks[0] = Create(TCP_TYPE_ACTIVE);
  socks[1] = Create(TCP_TYPE_PASSIVE);
  Listen(socks[1]);
  Connect(socks[0], socks[1]);

  TransferData(socks[0], socks[1], data, sizeof(data));
  TransferData(socks[1], socks[0], data, sizeof(data));
}

TEST_F(MultiTcpSocketTest, TestTwoSendsBeforeReceives) {
  const char data1[] = "TestTwoSendsBeforeReceives";
  const char data2[] = "2nd data";
  socks[0] = Create(TCP_TYPE_ACTIVE);
  socks[1] = Create(TCP_TYPE_PASSIVE);
  Listen(socks[1]);
  Connect(socks[0], socks[1]);

  SendData(socks[0], socks[1], data1, sizeof(data1));
  SendData(socks[0], socks[1], data2, sizeof(data2));
  RecvData(socks[0], socks[1], data1, sizeof(data1));
  /* ICE TCP framing turns TCP effectively into datagram mode */
  RecvData(socks[0], socks[1], data2, sizeof(data2));
}

TEST_F(MultiTcpSocketTest, TestTwoActiveBidirectionalTransmit) {
  const char data1[] = "TestTwoActiveBidirectionalTransmit";
  const char data2[] = "ReplyToTheFirstSocket";
  const char data3[] = "TestMessageFromTheSecondSocket";
  const char data4[] = "ThisIsAReplyToTheSecondSocket";
  socks[0] = Create(TCP_TYPE_PASSIVE);
  socks[1] = Create(TCP_TYPE_ACTIVE);
  socks[2] = Create(TCP_TYPE_ACTIVE);
  Listen(socks[0]);
  Connect(socks[1], socks[0]);
  Connect(socks[2], socks[0]);

  TransferData(socks[1], socks[0], data1, sizeof(data1));
  TransferData(socks[0], socks[1], data2, sizeof(data2));
  TransferData(socks[2], socks[0], data3, sizeof(data3));
  TransferData(socks[0], socks[2], data4, sizeof(data4));
}

TEST_F(MultiTcpSocketTest, TestTwoPassiveBidirectionalTransmit) {
  const char data1[] = "TestTwoPassiveBidirectionalTransmit";
  const char data2[] = "FirstReply";
  const char data3[] = "TestTwoPassiveBidirectionalTransmitToTheSecondSock";
  const char data4[] = "SecondReply";
  socks[0] = Create(TCP_TYPE_PASSIVE);
  socks[1] = Create(TCP_TYPE_PASSIVE);
  socks[2] = Create(TCP_TYPE_ACTIVE);
  Listen(socks[0]);
  Listen(socks[1]);
  Connect(socks[2], socks[0]);
  Connect(socks[2], socks[1]);

  TransferData(socks[2], socks[0], data1, sizeof(data1));
  TransferData(socks[0], socks[2], data2, sizeof(data2));
  TransferData(socks[2], socks[1], data3, sizeof(data3));
  TransferData(socks[1], socks[2], data4, sizeof(data4));
}

TEST_F(MultiTcpSocketTest, TestActivePassiveWithStunServerMockup) {
  /* Fake STUN message able to pass the nr_is_stun_msg check
     used in nr_socket_buffered_stun */
  const char stunMessage[] = {
    '\x00', '\x01', '\x00', '\x04', '\x21', '\x12', '\xa4', '\x42',
    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x0c', '\x00',
    '\x00', '\x00', '\x00', '\x00', '\x1c', '\xed', '\xca', '\xfe'
  };
  const char data[] = "TestActivePassiveWithStunServerMockup";

  socks[0] = Create(TCP_TYPE_PASSIVE, NULL, 0); // stun server socket
  Listen(socks[0]);

  socks[1] = Create(TCP_TYPE_PASSIVE, socks[0]);
  Listen(socks[1]);
  socks[2] = Create(TCP_TYPE_ACTIVE, socks[0]);

  TransferData(socks[1], socks[0], stunMessage, sizeof(stunMessage));
  TransferData(socks[0], socks[1], stunMessage, sizeof(stunMessage));
  Connect(socks[2], socks[1]);
  TransferData(socks[2], socks[1], data, sizeof(data));
  TransferData(socks[1], socks[2], data, sizeof(data));
}

TEST_F(MultiTcpSocketTest,  TestConnectTwoSo) {
  socks[0] = Create(TCP_TYPE_SO);
  socks[1] = Create(TCP_TYPE_SO);
  ConnectSo(socks[0], socks[1]);
}

// test works on localhost only with delay applied:
//   tc qdisc add dev lo root netem delay 5ms
TEST_F(MultiTcpSocketTest, DISABLED_TestTwoSoBidirectionalTransmit) {
  const char data[] = "TestTwoSoBidirectionalTransmit";
  socks[0] = Create(TCP_TYPE_SO);
  socks[1] = Create(TCP_TYPE_SO);
  ConnectSo(socks[0], socks[1]);
  TransferData(socks[0], socks[1], data, sizeof(data));
  TransferData(socks[1], socks[0], data, sizeof(data));
}

TEST_F(MultiTcpSocketTest, TestBigData) {
  char buf1[2048];
  char buf2[1024];

  for(unsigned i=0; i<sizeof(buf1); ++i) {
    buf1[i]=i&0xff;
  }
  for(unsigned i=0; i<sizeof(buf2); ++i) {
    buf2[i]=(i+0x80)&0xff;
  }
  socks[0] = Create(TCP_TYPE_ACTIVE);
  socks[1] = Create(TCP_TYPE_PASSIVE);
  Listen(socks[1]);
  Connect(socks[0], socks[1]);

  TransferData(socks[0], socks[1], buf1, sizeof(buf1));
  TransferData(socks[0], socks[1], buf2, sizeof(buf2));
// opposite dir
  SendData(socks[1], socks[0], buf2, sizeof(buf2));
  SendData(socks[1], socks[0], buf1, sizeof(buf1));
  RecvData(socks[1], socks[0], buf2, sizeof(buf2));
  RecvData(socks[1], socks[0], buf1, sizeof(buf1));
}


int main(int argc, char **argv)
{
  test_utils = new MtransportTestUtils();
  NSS_NoDB_Init(nullptr); // For random number generation

  ::testing::TestEventListeners& listeners =
        ::testing::UnitTest::GetInstance()->listeners();
  // Adds a listener to the end.  Google Test takes the ownership.
  listeners.Append(new test::RingbufferDumper(test_utils));

  // Start the tests
  ::testing::InitGoogleTest(&argc, argv);

  int rv = RUN_ALL_TESTS();

  delete test_utils;
  return rv;
}
