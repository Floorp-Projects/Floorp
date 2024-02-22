/*
 *  Copyright 2023 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/async_packet_socket.h"

#include "rtc_base/third_party/sigslot/sigslot.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace rtc {
namespace {

using ::testing::MockFunction;

class MockAsyncPacketSocket : public rtc::AsyncPacketSocket {
 public:
  ~MockAsyncPacketSocket() = default;

  MOCK_METHOD(SocketAddress, GetLocalAddress, (), (const, override));
  MOCK_METHOD(SocketAddress, GetRemoteAddress, (), (const, override));
  MOCK_METHOD(int,
              Send,
              (const void* pv, size_t cb, const rtc::PacketOptions& options),
              (override));

  MOCK_METHOD(int,
              SendTo,
              (const void* pv,
               size_t cb,
               const SocketAddress& addr,
               const rtc::PacketOptions& options),
              (override));
  MOCK_METHOD(int, Close, (), (override));
  MOCK_METHOD(State, GetState, (), (const, override));
  MOCK_METHOD(int,
              GetOption,
              (rtc::Socket::Option opt, int* value),
              (override));
  MOCK_METHOD(int, SetOption, (rtc::Socket::Option opt, int value), (override));
  MOCK_METHOD(int, GetError, (), (const, override));
  MOCK_METHOD(void, SetError, (int error), (override));

  void NotifyPacketReceived() {
    char data[1] = {'a'};
    AsyncPacketSocket::NotifyPacketReceived(this, data, 1, SocketAddress(), -1);
  }
};

TEST(AsyncPacketSocket, RegisteredCallbackReceivePacketsFromNotify) {
  MockAsyncPacketSocket mock_socket;
  MockFunction<void(AsyncPacketSocket*, const rtc::ReceivedPacket&)>
      received_packet;

  EXPECT_CALL(received_packet, Call);
  mock_socket.RegisterReceivedPacketCallback(received_packet.AsStdFunction());
  mock_socket.NotifyPacketReceived();
}

TEST(AsyncPacketSocket, RegisteredCallbackReceivePacketsFromSignalReadPacket) {
  MockAsyncPacketSocket mock_socket;
  MockFunction<void(AsyncPacketSocket*, const rtc::ReceivedPacket&)>
      received_packet;

  EXPECT_CALL(received_packet, Call);
  mock_socket.RegisterReceivedPacketCallback(received_packet.AsStdFunction());
  char data[1] = {'a'};
  mock_socket.SignalReadPacket(&mock_socket, data, 1, SocketAddress(), -1);
}

TEST(AsyncPacketSocket, SignalReadPacketTriggeredByNotifyPacketReceived) {
  class SigslotPacketReceiver : public sigslot::has_slots<> {
   public:
    explicit SigslotPacketReceiver(rtc::AsyncPacketSocket& socket) {
      socket.SignalReadPacket.connect(this,
                                      &SigslotPacketReceiver::OnPacketReceived);
    }

    bool packet_received() const { return packet_received_; }

   private:
    void OnPacketReceived(AsyncPacketSocket*,
                          const char*,
                          size_t,
                          const SocketAddress&,
                          // TODO(bugs.webrtc.org/9584): Change to passing the
                          // int64_t timestamp by value.
                          const int64_t&) {
      packet_received_ = true;
    }

    bool packet_received_ = false;
  };

  MockAsyncPacketSocket mock_socket;
  SigslotPacketReceiver receiver(mock_socket);
  ASSERT_FALSE(receiver.packet_received());

  mock_socket.NotifyPacketReceived();
  EXPECT_TRUE(receiver.packet_received());
}

}  // namespace
}  // namespace rtc
