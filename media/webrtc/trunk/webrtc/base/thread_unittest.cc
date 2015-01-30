/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/asyncinvoker.h"
#include "webrtc/base/asyncudpsocket.h"
#include "webrtc/base/event.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/physicalsocketserver.h"
#include "webrtc/base/socketaddress.h"
#include "webrtc/base/thread.h"
#include "webrtc/test/testsupport/gtest_disable.h"

#if defined(WEBRTC_WIN)
#include <comdef.h>  // NOLINT
#endif

using namespace rtc;

// Generates a sequence of numbers (collaboratively).
class TestGenerator {
 public:
  TestGenerator() : last(0), count(0) {}

  int Next(int prev) {
    int result = prev + last;
    last = result;
    count += 1;
    return result;
  }

  int last;
  int count;
};

struct TestMessage : public MessageData {
  explicit TestMessage(int v) : value(v) {}
  virtual ~TestMessage() {}

  int value;
};

// Receives on a socket and sends by posting messages.
class SocketClient : public TestGenerator, public sigslot::has_slots<> {
 public:
  SocketClient(AsyncSocket* socket, const SocketAddress& addr,
               Thread* post_thread, MessageHandler* phandler)
      : socket_(AsyncUDPSocket::Create(socket, addr)),
        post_thread_(post_thread),
        post_handler_(phandler) {
    socket_->SignalReadPacket.connect(this, &SocketClient::OnPacket);
  }

  ~SocketClient() {
    delete socket_;
  }

  SocketAddress address() const { return socket_->GetLocalAddress(); }

  void OnPacket(AsyncPacketSocket* socket, const char* buf, size_t size,
                const SocketAddress& remote_addr,
                const PacketTime& packet_time) {
    EXPECT_EQ(size, sizeof(uint32));
    uint32 prev = reinterpret_cast<const uint32*>(buf)[0];
    uint32 result = Next(prev);

    post_thread_->PostDelayed(200, post_handler_, 0, new TestMessage(result));
  }

 private:
  AsyncUDPSocket* socket_;
  Thread* post_thread_;
  MessageHandler* post_handler_;
};

// Receives messages and sends on a socket.
class MessageClient : public MessageHandler, public TestGenerator {
 public:
  MessageClient(Thread* pth, Socket* socket)
      : socket_(socket) {
  }

  virtual ~MessageClient() {
    delete socket_;
  }

  virtual void OnMessage(Message *pmsg) {
    TestMessage* msg = static_cast<TestMessage*>(pmsg->pdata);
    int result = Next(msg->value);
    EXPECT_GE(socket_->Send(&result, sizeof(result)), 0);
    delete msg;
  }

 private:
  Socket* socket_;
};

class CustomThread : public rtc::Thread {
 public:
  CustomThread() {}
  virtual ~CustomThread() { Stop(); }
  bool Start() { return false; }

  bool WrapCurrent() {
    return Thread::WrapCurrent();
  }
  void UnwrapCurrent() {
    Thread::UnwrapCurrent();
  }
};


// A thread that does nothing when it runs and signals an event
// when it is destroyed.
class SignalWhenDestroyedThread : public Thread {
 public:
  SignalWhenDestroyedThread(Event* event)
      : event_(event) {
  }

  virtual ~SignalWhenDestroyedThread() {
    Stop();
    event_->Set();
  }

  virtual void Run() {
    // Do nothing.
  }

 private:
  Event* event_;
};

// Function objects to test Thread::Invoke.
struct FunctorA {
  int operator()() { return 42; }
};
class FunctorB {
 public:
  explicit FunctorB(bool* flag) : flag_(flag) {}
  void operator()() { if (flag_) *flag_ = true; }
 private:
  bool* flag_;
};
struct FunctorC {
  int operator()() {
    Thread::Current()->ProcessMessages(50);
    return 24;
  }
};

// See: https://code.google.com/p/webrtc/issues/detail?id=2409
TEST(ThreadTest, DISABLED_Main) {
  const SocketAddress addr("127.0.0.1", 0);

  // Create the messaging client on its own thread.
  Thread th1;
  Socket* socket = th1.socketserver()->CreateAsyncSocket(addr.family(),
                                                         SOCK_DGRAM);
  MessageClient msg_client(&th1, socket);

  // Create the socket client on its own thread.
  Thread th2;
  AsyncSocket* asocket =
      th2.socketserver()->CreateAsyncSocket(addr.family(), SOCK_DGRAM);
  SocketClient sock_client(asocket, addr, &th1, &msg_client);

  socket->Connect(sock_client.address());

  th1.Start();
  th2.Start();

  // Get the messages started.
  th1.PostDelayed(100, &msg_client, 0, new TestMessage(1));

  // Give the clients a little while to run.
  // Messages will be processed at 100, 300, 500, 700, 900.
  Thread* th_main = Thread::Current();
  th_main->ProcessMessages(1000);

  // Stop the sending client. Give the receiver a bit longer to run, in case
  // it is running on a machine that is under load (e.g. the build machine).
  th1.Stop();
  th_main->ProcessMessages(200);
  th2.Stop();

  // Make sure the results were correct
  EXPECT_EQ(5, msg_client.count);
  EXPECT_EQ(34, msg_client.last);
  EXPECT_EQ(5, sock_client.count);
  EXPECT_EQ(55, sock_client.last);
}

// Test that setting thread names doesn't cause a malfunction.
// There's no easy way to verify the name was set properly at this time.
TEST(ThreadTest, Names) {
  // Default name
  Thread *thread;
  thread = new Thread();
  EXPECT_TRUE(thread->Start());
  thread->Stop();
  delete thread;
  thread = new Thread();
  // Name with no object parameter
  EXPECT_TRUE(thread->SetName("No object", NULL));
  EXPECT_TRUE(thread->Start());
  thread->Stop();
  delete thread;
  // Really long name
  thread = new Thread();
  EXPECT_TRUE(thread->SetName("Abcdefghijklmnopqrstuvwxyz1234567890", this));
  EXPECT_TRUE(thread->Start());
  thread->Stop();
  delete thread;
}

// Test that setting thread priorities doesn't cause a malfunction.
// There's no easy way to verify the priority was set properly at this time.
TEST(ThreadTest, Priorities) {
  Thread *thread;
  thread = new Thread();
  EXPECT_TRUE(thread->SetPriority(PRIORITY_HIGH));
  EXPECT_TRUE(thread->Start());
  thread->Stop();
  delete thread;
  thread = new Thread();
  EXPECT_TRUE(thread->SetPriority(PRIORITY_ABOVE_NORMAL));
  EXPECT_TRUE(thread->Start());
  thread->Stop();
  delete thread;

  thread = new Thread();
  EXPECT_TRUE(thread->Start());
#if defined(WEBRTC_WIN)
  EXPECT_TRUE(thread->SetPriority(PRIORITY_ABOVE_NORMAL));
#else
  EXPECT_FALSE(thread->SetPriority(PRIORITY_ABOVE_NORMAL));
#endif
  thread->Stop();
  delete thread;

}

TEST(ThreadTest, Wrap) {
  Thread* current_thread = Thread::Current();
  current_thread->UnwrapCurrent();
  CustomThread* cthread = new CustomThread();
  EXPECT_TRUE(cthread->WrapCurrent());
  EXPECT_TRUE(cthread->RunningForTest());
  EXPECT_FALSE(cthread->IsOwned());
  cthread->UnwrapCurrent();
  EXPECT_FALSE(cthread->RunningForTest());
  delete cthread;
  current_thread->WrapCurrent();
}

TEST(ThreadTest, Invoke) {
  // Create and start the thread.
  Thread thread;
  thread.Start();
  // Try calling functors.
  EXPECT_EQ(42, thread.Invoke<int>(FunctorA()));
  bool called = false;
  FunctorB f2(&called);
  thread.Invoke<void>(f2);
  EXPECT_TRUE(called);
  // Try calling bare functions.
  struct LocalFuncs {
    static int Func1() { return 999; }
    static void Func2() {}
  };
  EXPECT_EQ(999, thread.Invoke<int>(&LocalFuncs::Func1));
  thread.Invoke<void>(&LocalFuncs::Func2);
}

// Verifies that two threads calling Invoke on each other at the same time does
// not deadlock.
TEST(ThreadTest, TwoThreadsInvokeNoDeadlock) {
  AutoThread thread;
  Thread* current_thread = Thread::Current();
  ASSERT_TRUE(current_thread != NULL);

  Thread other_thread;
  other_thread.Start();

  struct LocalFuncs {
    static void Set(bool* out) { *out = true; }
    static void InvokeSet(Thread* thread, bool* out) {
      thread->Invoke<void>(Bind(&Set, out));
    }
  };

  bool called = false;
  other_thread.Invoke<void>(
      Bind(&LocalFuncs::InvokeSet, current_thread, &called));

  EXPECT_TRUE(called);
}

// Verifies that if thread A invokes a call on thread B and thread C is trying
// to invoke A at the same time, thread A does not handle C's invoke while
// invoking B.
TEST(ThreadTest, ThreeThreadsInvoke) {
  AutoThread thread;
  Thread* thread_a = Thread::Current();
  Thread thread_b, thread_c;
  thread_b.Start();
  thread_c.Start();

  class LockedBool {
   public:
    explicit LockedBool(bool value) : value_(value) {}

    void Set(bool value) {
      CritScope lock(&crit_);
      value_ = value;
    }

    bool Get() {
      CritScope lock(&crit_);
      return value_;
    }

   private:
    CriticalSection crit_;
    bool value_ GUARDED_BY(crit_);
  };

  struct LocalFuncs {
    static void Set(LockedBool* out) { out->Set(true); }
    static void InvokeSet(Thread* thread, LockedBool* out) {
      thread->Invoke<void>(Bind(&Set, out));
    }

    // Set |out| true and call InvokeSet on |thread|.
    static void SetAndInvokeSet(LockedBool* out,
                                Thread* thread,
                                LockedBool* out_inner) {
      out->Set(true);
      InvokeSet(thread, out_inner);
    }

    // Asynchronously invoke SetAndInvokeSet on |thread1| and wait until
    // |thread1| starts the call.
    static void AsyncInvokeSetAndWait(
        Thread* thread1, Thread* thread2, LockedBool* out) {
      CriticalSection crit;
      LockedBool async_invoked(false);

      AsyncInvoker invoker;
      invoker.AsyncInvoke<void>(
          thread1, Bind(&SetAndInvokeSet, &async_invoked, thread2, out));

      EXPECT_TRUE_WAIT(async_invoked.Get(), 2000);
    }
  };

  LockedBool thread_a_called(false);

  // Start the sequence A --(invoke)--> B --(async invoke)--> C --(invoke)--> A.
  // Thread B returns when C receives the call and C should be blocked until A
  // starts to process messages.
  thread_b.Invoke<void>(Bind(&LocalFuncs::AsyncInvokeSetAndWait,
                             &thread_c, thread_a, &thread_a_called));
  EXPECT_FALSE(thread_a_called.Get());

  EXPECT_TRUE_WAIT(thread_a_called.Get(), 2000);
}

class AsyncInvokeTest : public testing::Test {
 public:
  void IntCallback(int value) {
    EXPECT_EQ(expected_thread_, Thread::Current());
    int_value_ = value;
  }
  void AsyncInvokeIntCallback(AsyncInvoker* invoker, Thread* thread) {
    expected_thread_ = thread;
    invoker->AsyncInvoke(thread, FunctorC(),
                         &AsyncInvokeTest::IntCallback,
                         static_cast<AsyncInvokeTest*>(this));
    invoke_started_.Set();
  }
  void SetExpectedThreadForIntCallback(Thread* thread) {
    expected_thread_ = thread;
  }

 protected:
  enum { kWaitTimeout = 1000 };
  AsyncInvokeTest()
      : int_value_(0),
        invoke_started_(true, false),
        expected_thread_(NULL) {}

  int int_value_;
  Event invoke_started_;
  Thread* expected_thread_;
};

TEST_F(AsyncInvokeTest, FireAndForget) {
  AsyncInvoker invoker;
  // Create and start the thread.
  Thread thread;
  thread.Start();
  // Try calling functor.
  bool called = false;
  invoker.AsyncInvoke<void>(&thread, FunctorB(&called));
  EXPECT_TRUE_WAIT(called, kWaitTimeout);
}

TEST_F(AsyncInvokeTest, WithCallback) {
  AsyncInvoker invoker;
  // Create and start the thread.
  Thread thread;
  thread.Start();
  // Try calling functor.
  SetExpectedThreadForIntCallback(Thread::Current());
  invoker.AsyncInvoke(&thread, FunctorA(),
                      &AsyncInvokeTest::IntCallback,
                      static_cast<AsyncInvokeTest*>(this));
  EXPECT_EQ_WAIT(42, int_value_, kWaitTimeout);
}

TEST_F(AsyncInvokeTest, CancelInvoker) {
  // Create and start the thread.
  Thread thread;
  thread.Start();
  // Try destroying invoker during call.
  {
    AsyncInvoker invoker;
    invoker.AsyncInvoke(&thread, FunctorC(),
                        &AsyncInvokeTest::IntCallback,
                        static_cast<AsyncInvokeTest*>(this));
  }
  // With invoker gone, callback should be cancelled.
  Thread::Current()->ProcessMessages(kWaitTimeout);
  EXPECT_EQ(0, int_value_);
}

TEST_F(AsyncInvokeTest, CancelCallingThread) {
  AsyncInvoker invoker;
  { // Create and start the thread.
    Thread thread;
    thread.Start();
    // Try calling functor.
    thread.Invoke<void>(Bind(&AsyncInvokeTest::AsyncInvokeIntCallback,
                             static_cast<AsyncInvokeTest*>(this),
                             &invoker, Thread::Current()));
    // Wait for the call to begin.
    ASSERT_TRUE(invoke_started_.Wait(kWaitTimeout));
  }
  // Calling thread is gone. Return message shouldn't happen.
  Thread::Current()->ProcessMessages(kWaitTimeout);
  EXPECT_EQ(0, int_value_);
}

TEST_F(AsyncInvokeTest, KillInvokerBeforeExecute) {
  Thread thread;
  thread.Start();
  {
    AsyncInvoker invoker;
    // Try calling functor.
    thread.Invoke<void>(Bind(&AsyncInvokeTest::AsyncInvokeIntCallback,
                             static_cast<AsyncInvokeTest*>(this),
                             &invoker, Thread::Current()));
    // Wait for the call to begin.
    ASSERT_TRUE(invoke_started_.Wait(kWaitTimeout));
  }
  // Invoker is destroyed. Function should not execute.
  Thread::Current()->ProcessMessages(kWaitTimeout);
  EXPECT_EQ(0, int_value_);
}

TEST_F(AsyncInvokeTest, Flush) {
  AsyncInvoker invoker;
  bool flag1 = false;
  bool flag2 = false;
  // Queue two async calls to the current thread.
  invoker.AsyncInvoke<void>(Thread::Current(),
                            FunctorB(&flag1));
  invoker.AsyncInvoke<void>(Thread::Current(),
                            FunctorB(&flag2));
  // Because we haven't pumped messages, these should not have run yet.
  EXPECT_FALSE(flag1);
  EXPECT_FALSE(flag2);
  // Force them to run now.
  invoker.Flush(Thread::Current());
  EXPECT_TRUE(flag1);
  EXPECT_TRUE(flag2);
}

TEST_F(AsyncInvokeTest, FlushWithIds) {
  AsyncInvoker invoker;
  bool flag1 = false;
  bool flag2 = false;
  // Queue two async calls to the current thread, one with a message id.
  invoker.AsyncInvoke<void>(Thread::Current(),
                            FunctorB(&flag1),
                            5);
  invoker.AsyncInvoke<void>(Thread::Current(),
                            FunctorB(&flag2));
  // Because we haven't pumped messages, these should not have run yet.
  EXPECT_FALSE(flag1);
  EXPECT_FALSE(flag2);
  // Execute pending calls with id == 5.
  invoker.Flush(Thread::Current(), 5);
  EXPECT_TRUE(flag1);
  EXPECT_FALSE(flag2);
  flag1 = false;
  // Execute all pending calls. The id == 5 call should not execute again.
  invoker.Flush(Thread::Current());
  EXPECT_FALSE(flag1);
  EXPECT_TRUE(flag2);
}


#if defined(WEBRTC_WIN)
class ComThreadTest : public testing::Test, public MessageHandler {
 public:
  ComThreadTest() : done_(false) {}
 protected:
  virtual void OnMessage(Message* message) {
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    // S_FALSE means the thread was already inited for a multithread apartment.
    EXPECT_EQ(S_FALSE, hr);
    if (SUCCEEDED(hr)) {
      CoUninitialize();
    }
    done_ = true;
  }
  bool done_;
};

TEST_F(ComThreadTest, ComInited) {
  Thread* thread = new ComThread();
  EXPECT_TRUE(thread->Start());
  thread->Post(this, 0);
  EXPECT_TRUE_WAIT(done_, 1000);
  delete thread;
}
#endif
