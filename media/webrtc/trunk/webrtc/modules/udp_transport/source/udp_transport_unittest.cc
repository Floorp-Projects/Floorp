/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <vector>

#include "udp_transport.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
// We include the implementation header file to get at the dependency-injecting
// constructor.
#include "udp_transport_impl.h"
// We must mock the socket manager, for which we need its definition.
#include "udp_socket_manager_wrapper.h"

using ::testing::_;
using ::testing::Return;

class MockUdpSocketWrapper : public webrtc::UdpSocketWrapper {
 public:
  // The following methods have to be mocked because they are pure.
  MOCK_METHOD1(ChangeUniqueId, WebRtc_Word32(WebRtc_Word32));
  MOCK_METHOD2(SetCallback, bool(webrtc::CallbackObj,
                                 webrtc::IncomingSocketCallback));
  MOCK_METHOD1(Bind, bool(const webrtc::SocketAddress&));
  MOCK_METHOD0(ValidHandle, bool());
  MOCK_METHOD4(SetSockopt, bool(WebRtc_Word32, WebRtc_Word32,
                                const WebRtc_Word8*,
                                WebRtc_Word32));
  MOCK_METHOD1(SetTOS, WebRtc_Word32(WebRtc_Word32));
  MOCK_METHOD3(SendTo, WebRtc_Word32(const WebRtc_Word8*, WebRtc_Word32,
                                     const webrtc::SocketAddress&));
  MOCK_METHOD8(SetQos, bool(WebRtc_Word32, WebRtc_Word32,
                            WebRtc_Word32, WebRtc_Word32,
                            WebRtc_Word32, WebRtc_Word32,
                            const webrtc::SocketAddress &,
                            WebRtc_Word32));
};

class MockUdpSocketManager : public webrtc::UdpSocketManager {
 public:
  // Access to protected destructor.
  void Destroy() {
    delete this;
  }
  MOCK_METHOD2(Init, bool(WebRtc_Word32, WebRtc_UWord8&));
  MOCK_METHOD1(ChangeUniqueId, WebRtc_Word32(const WebRtc_Word32));
  MOCK_METHOD0(Start, bool());
  MOCK_METHOD0(Stop, bool());
  MOCK_METHOD1(AddSocket, bool(webrtc::UdpSocketWrapper*));
  MOCK_METHOD1(RemoveSocket, bool(webrtc::UdpSocketWrapper*));
};

class MockSocketFactory :
    public webrtc::UdpTransportImpl::SocketFactoryInterface {
 public:
  MockSocketFactory(std::vector<MockUdpSocketWrapper*>* socket_counter)
      : socket_counter_(socket_counter) {
  }
  webrtc::UdpSocketWrapper* CreateSocket(const WebRtc_Word32 id,
                                         webrtc::UdpSocketManager* mgr,
                                         webrtc::CallbackObj obj,
                                         webrtc::IncomingSocketCallback cb,
                                         bool ipV6Enable,
                                         bool disableGQOS) {
    MockUdpSocketWrapper* socket = new MockUdpSocketWrapper();
    // We instrument the socket with calls that are expected, but do
    // not matter for any specific test, in order to avoid warning messages.
    EXPECT_CALL(*socket, ValidHandle()).WillRepeatedly(Return(true));
    EXPECT_CALL(*socket, Bind(_)).WillOnce(Return(true));
    socket_counter_->push_back(socket);
    return socket;
  }
  std::vector<MockUdpSocketWrapper*>* socket_counter_;
};

class UDPTransportTest : public ::testing::Test {
 public:
  UDPTransportTest()
      : sockets_created_(0) {
  }

  ~UDPTransportTest() {
    // In production, sockets register themselves at creation time with
    // an UdpSocketManager, and the UdpSocketManager is responsible for
    // deleting them. In this test, we just delete them after the test.
    while (!sockets_created_.empty()) {
      delete sockets_created_.back();
      sockets_created_.pop_back();
    }
  }

  int NumSocketsCreated() {
    return sockets_created_.size();
  }

  std::vector<MockUdpSocketWrapper*>* sockets_created() {
    return &sockets_created_;
  }
private:
  std::vector<MockUdpSocketWrapper*> sockets_created_;
};

TEST_F(UDPTransportTest, CreateTransport) {
  WebRtc_Word32 id = 0;
  WebRtc_UWord8 threads = 1;
  webrtc::UdpTransport* transport = webrtc::UdpTransport::Create(id, threads);
  webrtc::UdpTransport::Destroy(transport);
}

// This test verifies that the mock_socket is not called from the constructor.
TEST_F(UDPTransportTest, ConstructorDoesNotCreateSocket) {
  WebRtc_Word32 id = 0;
  webrtc::UdpTransportImpl::SocketFactoryInterface* null_maker = NULL;
  webrtc::UdpSocketManager* null_manager = NULL;
  webrtc::UdpTransport* transport = new webrtc::UdpTransportImpl(id,
                                                                 null_maker,
                                                                 null_manager);
  delete transport;
}

TEST_F(UDPTransportTest, InitializeSourcePorts) {
  WebRtc_Word32 id = 0;
  webrtc::UdpTransportImpl::SocketFactoryInterface* mock_maker
      = new MockSocketFactory(sockets_created());
  MockUdpSocketManager* mock_manager = new MockUdpSocketManager();
  webrtc::UdpTransport* transport = new webrtc::UdpTransportImpl(id,
                                                                 mock_maker,
                                                                 mock_manager);
  EXPECT_EQ(0, transport->InitializeSourcePorts(4711, 4712));
  EXPECT_EQ(2, NumSocketsCreated());

  delete transport;
  mock_manager->Destroy();
}
