/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Tests for the UdpSocketWrapper interface.
// This will test the UdpSocket implementations on various platforms.
// Note that this test is using a real SocketManager, which starts up
// an extra worker thread, making the testing more complex than it
// should be.
// This is because on Posix, the CloseBlocking function waits for the
// ReadyForDeletion function to be called, which has to be called after
// CloseBlocking, and thus has to be called from another thread.
// The manager is the one actually doing the deleting.
// This is done differently in the Winsock2 code, but that code
// will also hang if the destructor is called directly.

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "modules/udp_transport/source/udp_socket_wrapper.h"
#include "modules/udp_transport/source/udp_socket_manager_wrapper.h"

using ::testing::_;
using ::testing::Return;

namespace webrtc {

class MockSocketManager : public UdpSocketManager {
 public:
  MockSocketManager() {}
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

// Creates a socket using the static constructor method and verifies that
// it's added to the socket manager.
TEST(UdpSocketWrapper, CreateSocket) {
  WebRtc_Word32 id = 42;
  // We can't test deletion of sockets without a socket manager.
  WebRtc_UWord8 threads = 1;
  UdpSocketManager* mgr = UdpSocketManager::Create(id, threads);
  UdpSocketWrapper* socket
       = UdpSocketWrapper::CreateSocket(id,
                                        mgr,
                                        NULL,  // CallbackObj
                                        NULL,  // IncomingSocketCallback
                                        false,  // ipV6Enable
                                        false);  // disableGQOS
  socket->CloseBlocking();
  UdpSocketManager::Return();
}

}  // namespace webrtc
