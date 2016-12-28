/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <signal.h>
#include <stdarg.h>

#include "webrtc/base/gunit.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/physicalsocketserver.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/socket_unittest.h"
#include "webrtc/base/testutils.h"
#include "webrtc/base/thread.h"

namespace rtc {

class PhysicalSocketTest;

class FakeSocketDispatcher : public SocketDispatcher {
 public:
  explicit FakeSocketDispatcher(PhysicalSocketServer* ss)
    : SocketDispatcher(ss) {
  }

 protected:
  SOCKET DoAccept(SOCKET socket, sockaddr* addr, socklen_t* addrlen) override;
};

class FakePhysicalSocketServer : public PhysicalSocketServer {
 public:
  explicit FakePhysicalSocketServer(PhysicalSocketTest* test)
    : test_(test) {
  }

  AsyncSocket* CreateAsyncSocket(int type) override {
    SocketDispatcher* dispatcher = new FakeSocketDispatcher(this);
    if (dispatcher->Create(type)) {
      return dispatcher;
    } else {
      delete dispatcher;
      return nullptr;
    }
  }

  AsyncSocket* CreateAsyncSocket(int family, int type) override {
    SocketDispatcher* dispatcher = new FakeSocketDispatcher(this);
    if (dispatcher->Create(family, type)) {
      return dispatcher;
    } else {
      delete dispatcher;
      return nullptr;
    }
  }

  PhysicalSocketTest* GetTest() const { return test_; }

 private:
  PhysicalSocketTest* test_;
};

class PhysicalSocketTest : public SocketTest {
 public:
  // Set flag to simluate failures when calling "::accept" on a AsyncSocket.
  void SetFailAccept(bool fail) { fail_accept_ = fail; }
  bool FailAccept() const { return fail_accept_; }

 protected:
  PhysicalSocketTest()
    : server_(new FakePhysicalSocketServer(this)),
      scope_(server_.get()),
      fail_accept_(false) {
  }

  void ConnectInternalAcceptError(const IPAddress& loopback);

  rtc::scoped_ptr<FakePhysicalSocketServer> server_;
  SocketServerScope scope_;
  bool fail_accept_;
};

SOCKET FakeSocketDispatcher::DoAccept(SOCKET socket,
                                      sockaddr* addr,
                                      socklen_t* addrlen) {
  FakePhysicalSocketServer* ss =
      static_cast<FakePhysicalSocketServer*>(socketserver());
  if (ss->GetTest()->FailAccept()) {
    return INVALID_SOCKET;
  }

  return SocketDispatcher::DoAccept(socket, addr, addrlen);
}

TEST_F(PhysicalSocketTest, TestConnectIPv4) {
  SocketTest::TestConnectIPv4();
}

// Crashes on Linux. See webrtc:4923.
#if defined(WEBRTC_LINUX)
#define MAYBE_TestConnectIPv6 DISABLED_TestConnectIPv6
#else
#define MAYBE_TestConnectIPv6 TestConnectIPv6
#endif
TEST_F(PhysicalSocketTest, MAYBE_TestConnectIPv6) {
  SocketTest::TestConnectIPv6();
}

TEST_F(PhysicalSocketTest, TestConnectWithDnsLookupIPv4) {
  SocketTest::TestConnectWithDnsLookupIPv4();
}

TEST_F(PhysicalSocketTest, TestConnectWithDnsLookupIPv6) {
  SocketTest::TestConnectWithDnsLookupIPv6();
}

TEST_F(PhysicalSocketTest, TestConnectFailIPv4) {
  SocketTest::TestConnectFailIPv4();
}

void PhysicalSocketTest::ConnectInternalAcceptError(const IPAddress& loopback) {
  testing::StreamSink sink;
  SocketAddress accept_addr;

  // Create two clients.
  scoped_ptr<AsyncSocket> client1(server_->CreateAsyncSocket(loopback.family(),
                                                             SOCK_STREAM));
  sink.Monitor(client1.get());
  EXPECT_EQ(AsyncSocket::CS_CLOSED, client1->GetState());
  EXPECT_PRED1(IsUnspecOrEmptyIP, client1->GetLocalAddress().ipaddr());

  scoped_ptr<AsyncSocket> client2(server_->CreateAsyncSocket(loopback.family(),
                                                             SOCK_STREAM));
  sink.Monitor(client2.get());
  EXPECT_EQ(AsyncSocket::CS_CLOSED, client2->GetState());
  EXPECT_PRED1(IsUnspecOrEmptyIP, client2->GetLocalAddress().ipaddr());

  // Create server and listen.
  scoped_ptr<AsyncSocket> server(
      server_->CreateAsyncSocket(loopback.family(), SOCK_STREAM));
  sink.Monitor(server.get());
  EXPECT_EQ(0, server->Bind(SocketAddress(loopback, 0)));
  EXPECT_EQ(0, server->Listen(5));
  EXPECT_EQ(AsyncSocket::CS_CONNECTING, server->GetState());

  // Ensure no pending server connections, since we haven't done anything yet.
  EXPECT_FALSE(sink.Check(server.get(), testing::SSE_READ));
  EXPECT_TRUE(nullptr == server->Accept(&accept_addr));
  EXPECT_TRUE(accept_addr.IsNil());

  // Attempt first connect to listening socket.
  EXPECT_EQ(0, client1->Connect(server->GetLocalAddress()));
  EXPECT_FALSE(client1->GetLocalAddress().IsNil());
  EXPECT_NE(server->GetLocalAddress(), client1->GetLocalAddress());

  // Client is connecting, outcome not yet determined.
  EXPECT_EQ(AsyncSocket::CS_CONNECTING, client1->GetState());
  EXPECT_FALSE(sink.Check(client1.get(), testing::SSE_OPEN));
  EXPECT_FALSE(sink.Check(client1.get(), testing::SSE_CLOSE));

  // Server has pending connection, try to accept it (will fail).
  EXPECT_TRUE_WAIT((sink.Check(server.get(), testing::SSE_READ)), kTimeout);
  // Simulate "::accept" returning an error.
  SetFailAccept(true);
  scoped_ptr<AsyncSocket> accepted(server->Accept(&accept_addr));
  EXPECT_FALSE(accepted);
  ASSERT_TRUE(accept_addr.IsNil());

  // Ensure no more pending server connections.
  EXPECT_FALSE(sink.Check(server.get(), testing::SSE_READ));
  EXPECT_TRUE(nullptr == server->Accept(&accept_addr));
  EXPECT_TRUE(accept_addr.IsNil());

  // Attempt second connect to listening socket.
  EXPECT_EQ(0, client2->Connect(server->GetLocalAddress()));
  EXPECT_FALSE(client2->GetLocalAddress().IsNil());
  EXPECT_NE(server->GetLocalAddress(), client2->GetLocalAddress());

  // Client is connecting, outcome not yet determined.
  EXPECT_EQ(AsyncSocket::CS_CONNECTING, client2->GetState());
  EXPECT_FALSE(sink.Check(client2.get(), testing::SSE_OPEN));
  EXPECT_FALSE(sink.Check(client2.get(), testing::SSE_CLOSE));

  // Server has pending connection, try to accept it (will succeed).
  EXPECT_TRUE_WAIT((sink.Check(server.get(), testing::SSE_READ)), kTimeout);
  SetFailAccept(false);
  scoped_ptr<AsyncSocket> accepted2(server->Accept(&accept_addr));
  ASSERT_TRUE(accepted2);
  EXPECT_FALSE(accept_addr.IsNil());
  EXPECT_EQ(accepted2->GetRemoteAddress(), accept_addr);
}

TEST_F(PhysicalSocketTest, TestConnectAcceptErrorIPv4) {
  ConnectInternalAcceptError(kIPv4Loopback);
}

// Crashes on Linux. See webrtc:4923.
#if defined(WEBRTC_LINUX)
#define MAYBE_TestConnectAcceptErrorIPv6 DISABLED_TestConnectAcceptErrorIPv6
#else
#define MAYBE_TestConnectAcceptErrorIPv6 TestConnectAcceptErrorIPv6
#endif
TEST_F(PhysicalSocketTest, MAYBE_TestConnectAcceptErrorIPv6) {
  ConnectInternalAcceptError(kIPv6Loopback);
}

// Crashes on Linux. See webrtc:4923.
#if defined(WEBRTC_LINUX)
#define MAYBE_TestConnectFailIPv6 DISABLED_TestConnectFailIPv6
#else
#define MAYBE_TestConnectFailIPv6 TestConnectFailIPv6
#endif
TEST_F(PhysicalSocketTest, MAYBE_TestConnectFailIPv6) {
  SocketTest::TestConnectFailIPv6();
}

TEST_F(PhysicalSocketTest, TestConnectWithDnsLookupFailIPv4) {
  SocketTest::TestConnectWithDnsLookupFailIPv4();
}

// Crashes on Linux. See webrtc:4923.
#if defined(WEBRTC_LINUX)
#define MAYBE_TestConnectWithDnsLookupFailIPv6 \
  DISABLED_TestConnectWithDnsLookupFailIPv6
#else
#define MAYBE_TestConnectWithDnsLookupFailIPv6 \
  TestConnectWithDnsLookupFailIPv6
#endif
TEST_F(PhysicalSocketTest, MAYBE_TestConnectWithDnsLookupFailIPv6) {
  SocketTest::TestConnectWithDnsLookupFailIPv6();
}


TEST_F(PhysicalSocketTest, TestConnectWithClosedSocketIPv4) {
  SocketTest::TestConnectWithClosedSocketIPv4();
}

// Crashes on Linux. See webrtc:4923.
#if defined(WEBRTC_LINUX)
#define MAYBE_TestConnectWithClosedSocketIPv6 \
  DISABLED_TestConnectWithClosedSocketIPv6
#else
#define MAYBE_TestConnectWithClosedSocketIPv6 TestConnectWithClosedSocketIPv6
#endif
TEST_F(PhysicalSocketTest, MAYBE_TestConnectWithClosedSocketIPv6) {
  SocketTest::TestConnectWithClosedSocketIPv6();
}

TEST_F(PhysicalSocketTest, TestConnectWhileNotClosedIPv4) {
  SocketTest::TestConnectWhileNotClosedIPv4();
}

// Crashes on Linux. See webrtc:4923.
#if defined(WEBRTC_LINUX)
#define MAYBE_TestConnectWhileNotClosedIPv6 \
  DISABLED_TestConnectWhileNotClosedIPv6
#else
#define MAYBE_TestConnectWhileNotClosedIPv6 TestConnectWhileNotClosedIPv6
#endif
TEST_F(PhysicalSocketTest, MAYBE_TestConnectWhileNotClosedIPv6) {
  SocketTest::TestConnectWhileNotClosedIPv6();
}

TEST_F(PhysicalSocketTest, TestServerCloseDuringConnectIPv4) {
  SocketTest::TestServerCloseDuringConnectIPv4();
}

// Crashes on Linux. See webrtc:4923.
#if defined(WEBRTC_LINUX)
#define MAYBE_TestServerCloseDuringConnectIPv6 \
  DISABLED_TestServerCloseDuringConnectIPv6
#else
#define MAYBE_TestServerCloseDuringConnectIPv6 TestServerCloseDuringConnectIPv6
#endif
TEST_F(PhysicalSocketTest, MAYBE_TestServerCloseDuringConnectIPv6) {
  SocketTest::TestServerCloseDuringConnectIPv6();
}

TEST_F(PhysicalSocketTest, TestClientCloseDuringConnectIPv4) {
  SocketTest::TestClientCloseDuringConnectIPv4();
}

// Crashes on Linux. See webrtc:4923.
#if defined(WEBRTC_LINUX)
#define MAYBE_TestClientCloseDuringConnectIPv6 \
  DISABLED_TestClientCloseDuringConnectIPv6
#else
#define MAYBE_TestClientCloseDuringConnectIPv6 TestClientCloseDuringConnectIPv6
#endif
TEST_F(PhysicalSocketTest, MAYBE_TestClientCloseDuringConnectIPv6) {
  SocketTest::TestClientCloseDuringConnectIPv6();
}

TEST_F(PhysicalSocketTest, TestServerCloseIPv4) {
  SocketTest::TestServerCloseIPv4();
}

// Crashes on Linux. See webrtc:4923.
#if defined(WEBRTC_LINUX)
#define MAYBE_TestServerCloseIPv6 DISABLED_TestServerCloseIPv6
#else
#define MAYBE_TestServerCloseIPv6 TestServerCloseIPv6
#endif
TEST_F(PhysicalSocketTest, MAYBE_TestServerCloseIPv6) {
  SocketTest::TestServerCloseIPv6();
}

TEST_F(PhysicalSocketTest, TestCloseInClosedCallbackIPv4) {
  SocketTest::TestCloseInClosedCallbackIPv4();
}

// Crashes on Linux. See webrtc:4923.
#if defined(WEBRTC_LINUX)
#define MAYBE_TestCloseInClosedCallbackIPv6 \
  DISABLED_TestCloseInClosedCallbackIPv6
#else
#define MAYBE_TestCloseInClosedCallbackIPv6 TestCloseInClosedCallbackIPv6
#endif
TEST_F(PhysicalSocketTest, MAYBE_TestCloseInClosedCallbackIPv6) {
  SocketTest::TestCloseInClosedCallbackIPv6();
}

TEST_F(PhysicalSocketTest, TestSocketServerWaitIPv4) {
  SocketTest::TestSocketServerWaitIPv4();
}

// Crashes on Linux. See webrtc:4923.
#if defined(WEBRTC_LINUX)
#define MAYBE_TestSocketServerWaitIPv6 DISABLED_TestSocketServerWaitIPv6
#else
#define MAYBE_TestSocketServerWaitIPv6 TestSocketServerWaitIPv6
#endif
TEST_F(PhysicalSocketTest, MAYBE_TestSocketServerWaitIPv6) {
  SocketTest::TestSocketServerWaitIPv6();
}

TEST_F(PhysicalSocketTest, TestTcpIPv4) {
  SocketTest::TestTcpIPv4();
}

// Crashes on Linux. See webrtc:4923.
#if defined(WEBRTC_LINUX)
#define MAYBE_TestTcpIPv6 DISABLED_TestTcpIPv6
#else
#define MAYBE_TestTcpIPv6 TestTcpIPv6
#endif
TEST_F(PhysicalSocketTest, MAYBE_TestTcpIPv6) {
  SocketTest::TestTcpIPv6();
}

TEST_F(PhysicalSocketTest, TestUdpIPv4) {
  SocketTest::TestUdpIPv4();
}

// Crashes on Linux. See webrtc:4923.
#if defined(WEBRTC_LINUX)
#define MAYBE_TestUdpIPv6 DISABLED_TestUdpIPv6
#else
#define MAYBE_TestUdpIPv6 TestUdpIPv6
#endif
TEST_F(PhysicalSocketTest, MAYBE_TestUdpIPv6) {
  SocketTest::TestUdpIPv6();
}

// Disable for TSan v2, see
// https://code.google.com/p/webrtc/issues/detail?id=3498 for details.
// Also disable for MSan, see:
// https://code.google.com/p/webrtc/issues/detail?id=4958
// TODO(deadbeef): Enable again once test is reimplemented to be unflaky.
// Also disable for ASan.
// Disabled on Android: https://code.google.com/p/webrtc/issues/detail?id=4364
// Disabled on Linux: https://bugs.chromium.org/p/webrtc/issues/detail?id=5233
#if defined(THREAD_SANITIZER) || defined(MEMORY_SANITIZER) || \
    defined(ADDRESS_SANITIZER) || defined(WEBRTC_ANDROID) ||  \
    defined(WEBRTC_LINUX)
#define MAYBE_TestUdpReadyToSendIPv4 DISABLED_TestUdpReadyToSendIPv4
#else
#define MAYBE_TestUdpReadyToSendIPv4 TestUdpReadyToSendIPv4
#endif
TEST_F(PhysicalSocketTest, MAYBE_TestUdpReadyToSendIPv4) {
  SocketTest::TestUdpReadyToSendIPv4();
}

TEST_F(PhysicalSocketTest, TestUdpReadyToSendIPv6) {
  SocketTest::TestUdpReadyToSendIPv6();
}

TEST_F(PhysicalSocketTest, TestGetSetOptionsIPv4) {
  SocketTest::TestGetSetOptionsIPv4();
}

TEST_F(PhysicalSocketTest, TestGetSetOptionsIPv6) {
  SocketTest::TestGetSetOptionsIPv6();
}

#if defined(WEBRTC_POSIX)

class PosixSignalDeliveryTest : public testing::Test {
 public:
  static void RecordSignal(int signum) {
    signals_received_.push_back(signum);
    signaled_thread_ = Thread::Current();
  }

 protected:
  void SetUp() {
    ss_.reset(new PhysicalSocketServer());
  }

  void TearDown() {
    ss_.reset(NULL);
    signals_received_.clear();
    signaled_thread_ = NULL;
  }

  bool ExpectSignal(int signum) {
    if (signals_received_.empty()) {
      LOG(LS_ERROR) << "ExpectSignal(): No signal received";
      return false;
    }
    if (signals_received_[0] != signum) {
      LOG(LS_ERROR) << "ExpectSignal(): Received signal " <<
          signals_received_[0] << ", expected " << signum;
      return false;
    }
    signals_received_.erase(signals_received_.begin());
    return true;
  }

  bool ExpectNone() {
    bool ret = signals_received_.empty();
    if (!ret) {
      LOG(LS_ERROR) << "ExpectNone(): Received signal " << signals_received_[0]
          << ", expected none";
    }
    return ret;
  }

  static std::vector<int> signals_received_;
  static Thread *signaled_thread_;

  scoped_ptr<PhysicalSocketServer> ss_;
};

std::vector<int> PosixSignalDeliveryTest::signals_received_;
Thread *PosixSignalDeliveryTest::signaled_thread_ = NULL;

// Test receiving a synchronous signal while not in Wait() and then entering
// Wait() afterwards.
TEST_F(PosixSignalDeliveryTest, RaiseThenWait) {
  ASSERT_TRUE(ss_->SetPosixSignalHandler(SIGTERM, &RecordSignal));
  raise(SIGTERM);
  EXPECT_TRUE(ss_->Wait(0, true));
  EXPECT_TRUE(ExpectSignal(SIGTERM));
  EXPECT_TRUE(ExpectNone());
}

// Test that we can handle getting tons of repeated signals and that we see all
// the different ones.
TEST_F(PosixSignalDeliveryTest, InsanelyManySignals) {
  ss_->SetPosixSignalHandler(SIGTERM, &RecordSignal);
  ss_->SetPosixSignalHandler(SIGINT, &RecordSignal);
  for (int i = 0; i < 10000; ++i) {
    raise(SIGTERM);
  }
  raise(SIGINT);
  EXPECT_TRUE(ss_->Wait(0, true));
  // Order will be lowest signal numbers first.
  EXPECT_TRUE(ExpectSignal(SIGINT));
  EXPECT_TRUE(ExpectSignal(SIGTERM));
  EXPECT_TRUE(ExpectNone());
}

// Test that a signal during a Wait() call is detected.
TEST_F(PosixSignalDeliveryTest, SignalDuringWait) {
  ss_->SetPosixSignalHandler(SIGALRM, &RecordSignal);
  alarm(1);
  EXPECT_TRUE(ss_->Wait(1500, true));
  EXPECT_TRUE(ExpectSignal(SIGALRM));
  EXPECT_TRUE(ExpectNone());
}

class RaiseSigTermRunnable : public Runnable {
  void Run(Thread *thread) {
    thread->socketserver()->Wait(1000, false);

    // Allow SIGTERM. This will be the only thread with it not masked so it will
    // be delivered to us.
    sigset_t mask;
    sigemptyset(&mask);
    pthread_sigmask(SIG_SETMASK, &mask, NULL);

    // Raise it.
    raise(SIGTERM);
  }
};

// Test that it works no matter what thread the kernel chooses to give the
// signal to (since it's not guaranteed to be the one that Wait() runs on).
TEST_F(PosixSignalDeliveryTest, SignalOnDifferentThread) {
  ss_->SetPosixSignalHandler(SIGTERM, &RecordSignal);
  // Mask out SIGTERM so that it can't be delivered to this thread.
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGTERM);
  EXPECT_EQ(0, pthread_sigmask(SIG_SETMASK, &mask, NULL));
  // Start a new thread that raises it. It will have to be delivered to that
  // thread. Our implementation should safely handle it and dispatch
  // RecordSignal() on this thread.
  scoped_ptr<Thread> thread(new Thread());
  scoped_ptr<RaiseSigTermRunnable> runnable(new RaiseSigTermRunnable());
  thread->Start(runnable.get());
  EXPECT_TRUE(ss_->Wait(1500, true));
  EXPECT_TRUE(ExpectSignal(SIGTERM));
  EXPECT_EQ(Thread::Current(), signaled_thread_);
  EXPECT_TRUE(ExpectNone());
}

#endif

}  // namespace rtc
