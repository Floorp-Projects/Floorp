/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include <minwindef.h>
#include <handleapi.h>
#include <synchapi.h>

#include "mozilla/Assertions.h"
#include "mozilla/ErrorNames.h"
#include "mozilla/Result.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/WinHandleWatcher.h"
#include "nsCOMPtr.h"
#include "nsError.h"
#include "nsIEventTarget.h"
#include "nsITargetShutdownTask.h"
#include "nsIThread.h"
#include "nsIThreadShutdown.h"
#include "nsITimer.h"
#include "nsTHashMap.h"
#include "nsThreadUtils.h"
// #include "nscore.h"

namespace details {
static nsCString MakeTargetName(const char* name) {
  const char* testName =
      ::testing::UnitTest::GetInstance()->current_test_info()->name();
  nsCString ret;
  ret.AppendPrintf("%s: %s", testName, name);
  return ret;
}
}  // namespace details

using HandleWatcher = mozilla::HandleWatcher;

///////////////////////////////////////////////////////////////////////
// Error handling

// nsresult_fatal_err_: auxiliary function for testing-macros.
[[noreturn]] void nsresult_fatal_err_(const char* file, size_t line,
                                      const char* expr, nsresult res) {
  // implementation details from the MOZ_CRASH* family of macros
  MOZ_Crash(file, static_cast<int>(line),
            MOZ_CrashPrintf("%s gave nsresult %s(%" PRIX32 ")", expr,
                            mozilla::GetStaticErrorName(res), uint32_t(res)));
}

// UNWRAP: testing-oriented variant of Result::unwrap.
//
// We make no use of gtest's `ASSERT_*` family of macros, since they assume
// that a `return;` statement is sufficient to abort the test.
template <typename T>
T unwrap_impl_(const char* file, size_t line, const char* expr,
               mozilla::Result<T, nsresult> res) {
  if (MOZ_LIKELY(res.isOk())) {
    return res.unwrap();
  }
  nsresult_fatal_err_(file, line, expr, res.unwrapErr());
}

#define UNWRAP(expr) unwrap_impl_(__FILE__, __LINE__, #expr, expr)

///////////////////////////////////////////////////////////////////////
// Milliseconds()
//
// Convenience declaration for millisecond-based mozilla::TimeDurations.
static mozilla::TimeDuration Milliseconds(double d) {
  return mozilla::TimeDuration::FromMilliseconds(d);
}

///////////////////////////////////////////////////////////////////////
// TestHandleWatcher
//
// GTest test fixture. Provides shared resources.
class TestHandleWatcher : public testing::Test {
 protected:
  static void SetUpTestSuite() { sIsLive = true; }
  static void TearDownTestSuite() {
    sPool = nullptr;
    sIsLive = false;
  }

 public:
  static already_AddRefed<mozilla::SharedThreadPool> GetPool() {
    AssertIsLive();
    if (!sPool) {
      sPool = mozilla::SharedThreadPool::Get("Test Pool"_ns);
    }
    return do_AddRef(sPool);
  }

 private:
  static bool sIsLive;  // just for confirmation
  static void AssertIsLive() {
    MOZ_ASSERT(sIsLive,
               "attempted to use `class TestHandleWatcher` outside test group");
  }

  static RefPtr<mozilla::SharedThreadPool> sPool;
};

/* static */
bool TestHandleWatcher::sIsLive = false;
/* static */
RefPtr<mozilla::SharedThreadPool> TestHandleWatcher::sPool = nullptr;

///////////////////////////////////////////////////////////////////////
// WindowsEventObject
//
// Convenient interface to a Windows `event` object. (This is a synchronization
// object that's usually the wrong thing to use.)
struct WindowsEventObject {
  HANDLE const handle = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);
  WindowsEventObject() = default;
  ~WindowsEventObject() { ::CloseHandle(handle); }

  WindowsEventObject(WindowsEventObject const&) = delete;
  WindowsEventObject(WindowsEventObject&&) = delete;
  WindowsEventObject& operator=(WindowsEventObject const&) = delete;
  WindowsEventObject& operator=(WindowsEventObject&&) = delete;

  void Set() { ::SetEvent(handle); }
};

///////////////////////////////////////////////////////////////////////
// SpawnNewThread
//
nsCOMPtr<nsIThread> SpawnNewThread(const char* name) {
  nsCOMPtr<nsIThread> thread;
  MOZ_ALWAYS_SUCCEEDS(
      NS_NewNamedThread(details::MakeTargetName(name), getter_AddRefs(thread)));
  return thread;
}

///////////////////////////////////////////////////////////////////////
// SpawnNewBackgroundQueue
//
// (mozilla::TaskQueue expects the supplied name to outlive the queue, so we
// just use a static string.)
RefPtr<mozilla::TaskQueue> SpawnNewBackgroundQueue() {
  return mozilla::TaskQueue::Create(TestHandleWatcher::GetPool(),
                                    "task queue for TestHandleWatcher");
}

///////////////////////////////////////////////////////////////////////
// SpinEventLoopUntil
//
// Local equivalent of `mozilla::SpinEventLoopUntil`, extended with a timeout.
//
// Spin the current thread's event loop until either a specified predicate is
// satisfied or a specified time-interval has passed.
//
struct SpinEventLoopUntilRet {
  enum Value { Ok, TimedOut, InternalError } value;
  bool ok() const { return value == Value::Ok; }
  bool timedOut() const { return value == Value::TimedOut; }

  MOZ_IMPLICIT SpinEventLoopUntilRet(Value v) : value(v) {}
};
template <typename Predicate>
SpinEventLoopUntilRet SpinEventLoopUntil(
    Predicate const& aPredicate,
    mozilla::TimeDuration aDuration = Milliseconds(500)) {
  using Value = SpinEventLoopUntilRet::Value;
  nsIThread* currentThread = NS_GetCurrentThread();

  // Set up timer.
  bool timedOut = false;
  auto timer = UNWRAP(NS_NewTimerWithCallback(
      [&](nsITimer*) { timedOut = true; }, aDuration, nsITimer::TYPE_ONE_SHOT,
      "SpinEventLoop timer", currentThread));
  auto onExitCancelTimer = mozilla::MakeScopeExit([&] { timer->Cancel(); });

  bool const ret = mozilla::SpinEventLoopUntil(
      "TestHandleWatcher"_ns, [&] { return timedOut || aPredicate(); });
  if (!ret) return Value::InternalError;
  if (timedOut) return Value::TimedOut;
  return Value::Ok;
}

// metatest for `SpinEventLoopUntil`
TEST_F(TestHandleWatcher, SpinEventLoopUntil) {
  auto should_fail = SpinEventLoopUntil([] { return false; }, Milliseconds(1));
  ASSERT_TRUE(should_fail.timedOut());
  auto should_pass = SpinEventLoopUntil([] { return true; }, Milliseconds(50));
  ASSERT_TRUE(should_pass.ok());
}

///////////////////////////////////////////////////////////////////////
// PingMainThread
//
// Post a do-nothing message to the main thread's event queue. (This will signal
// it to wake up and check its predicate, if it's waiting for that to happen.)
void PingMainThread() {
  MOZ_ALWAYS_SUCCEEDS(
      NS_DispatchToMainThread(NS_NewRunnableFunction("Ping", [] {})));
}

///////////////////////////////////////////////////////////////////////
// Individual tests

// Test basic creation and destruction.
TEST_F(TestHandleWatcher, Trivial) { HandleWatcher hw; }

// Test interaction before a Watch is created.
TEST_F(TestHandleWatcher, Empty) {
  HandleWatcher hw;
  ASSERT_TRUE(hw.IsStopped());
  hw.Stop();
}

// Start and trigger an HandleWatcher directly from the main thread.
TEST_F(TestHandleWatcher, Simple) {
  WindowsEventObject event;
  HandleWatcher hw;

  std::atomic<bool> run = false;

  hw.Watch(
      event.handle, NS_GetCurrentThread(),
      NS_NewRunnableFunction("TestHandleWatcher::Simple", [&] { run = true; }));

  ASSERT_FALSE(run.load());
  event.Set();
  // Attempt to force a race below.
  ::Sleep(0);
  // This should not race. The HandleWatcher doesn't execute its delegate; it
  // just queues a mozilla::Task to do that onto our thread's event queue,
  // and that Task hasn't been permitted to run yet.
  ASSERT_FALSE(run.load());

  ASSERT_TRUE(SpinEventLoopUntil([&] { return run.load(); }).ok());
}

// Test that calling Stop() stops the watcher.
TEST_F(TestHandleWatcher, Stop) {
  WindowsEventObject event;
  HandleWatcher hw;
  std::atomic<bool> run = false;

  hw.Watch(
      event.handle, NS_GetCurrentThread(),
      NS_NewRunnableFunction("TestHandleWatcher::Stop", [&] { run = true; }));

  ASSERT_FALSE(hw.IsStopped());
  hw.Stop();
  ASSERT_TRUE(hw.IsStopped());

  ASSERT_TRUE(SpinEventLoopUntil([&] { return run.load(); }, Milliseconds(25))
                  .timedOut());
}

// Test that the target's destruction stops the watch.
TEST_F(TestHandleWatcher, TargetDestroyed) {
  WindowsEventObject event;
  HandleWatcher hw;
  bool run = false;

  auto queue = SpawnNewThread("target thread");
  hw.Watch(event.handle, queue.get(),
           NS_NewRunnableFunction("never called", [&] { run = true; }));

  ASSERT_FALSE(hw.IsStopped());
  // synchronous shutdown before destruction
  queue->Shutdown();

  ASSERT_TRUE(hw.IsStopped());
  ASSERT_FALSE(run);
}

// Test that calling `Watch` again stops the current watch.
TEST_F(TestHandleWatcher, Rewatch) {
  WindowsEventObject event;
  HandleWatcher hw;

  bool b1 = false;
  bool b2 = false;

  {
    auto queue = SpawnNewThread("target thread");

    hw.Watch(event.handle, queue.get(), NS_NewRunnableFunction("b1", [&] {
               b1 = true;
               PingMainThread();
             }));
    hw.Watch(event.handle, queue.get(), NS_NewRunnableFunction("b2", [&] {
               b2 = true;
               PingMainThread();
             }));

    event.Set();

    ASSERT_TRUE(SpinEventLoopUntil([&] { return b1 || b2; }).ok());
    queue->Shutdown();
  }
  ASSERT_FALSE(b1);
  ASSERT_TRUE(b2);
}

// Test that watching a HANDLE which is _already_ signaled still fires the
// associated task.
TEST_F(TestHandleWatcher, Presignalled) {
  WindowsEventObject event;
  HandleWatcher hw;

  bool run = false;
  event.Set();
  hw.Watch(event.handle, NS_GetCurrentThread(),
           NS_NewRunnableFunction("TestHandleWatcher::Presignalled",
                                  [&] { run = true; }));

  ASSERT_TRUE(SpinEventLoopUntil([&] { return run; }).ok());
}

///////////////////////////////////////////////////////////////////////
// Systematic tests: normal activation
//
// Test that a handle becoming signalled on target A correctly enqueues a task
// on target B, regardless of whether A == B.
//
struct ActivationTestSetup {
  enum TargetType { Main, Side, Background };

  WindowsEventObject event;
  HandleWatcher watcher;

  std::atomic<bool> run = false;
  nsCOMPtr<nsIThread> sideThread;
  nsCOMPtr<nsISerialEventTarget> backgroundQueue;

 private:
  nsIEventTarget* GetQueue(TargetType targetTyoe) {
    MOZ_ASSERT(NS_IsMainThread());
    switch (targetTyoe) {
      case TargetType::Main:
        return NS_GetCurrentThread();

      case TargetType::Side: {
        if (!sideThread) {
          sideThread = SpawnNewThread("side thread");
        }
        return sideThread;
      }

      case TargetType::Background: {
        if (!backgroundQueue) {
          backgroundQueue = SpawnNewBackgroundQueue();
        }
        return backgroundQueue.get();
      }
    }
  }

  void OnSignaled() {
    run = true;
    // If we're not running on the main thread, it may be blocked waiting for
    // events.
    PingMainThread();
  }

 public:
  void Setup(TargetType from, TargetType to) {
    watcher.Watch(
        event.handle, GetQueue(to),
        NS_NewRunnableFunction("Reaction", [this] { this->OnSignaled(); }));

    MOZ_ALWAYS_SUCCEEDS(GetQueue(from)->Dispatch(
        NS_NewRunnableFunction("Action", [this] { event.Set(); })));
  }

  bool Execute() {
    MOZ_ASSERT(NS_IsMainThread());
    bool const spin = SpinEventLoopUntil([this] {
                        MOZ_ASSERT(NS_IsMainThread());
                        return run.load();
                      }).ok();
    return spin && watcher.IsStopped();
  }

  ~ActivationTestSetup() { watcher.Stop(); }
};

#define MOZ_HANDLEWATCHER_GTEST_FROM_TO(FROM, TO)  \
  TEST_F(TestHandleWatcher, FROM##To##TO) {        \
    ActivationTestSetup s;                         \
    s.Setup(ActivationTestSetup::TargetType::FROM, \
            ActivationTestSetup::TargetType::TO);  \
    ASSERT_TRUE(s.Execute());                      \
  }

// Note that `Main -> Main` is subtly different from `Simple`, above: `Simple`
// sets the event before spinning, while `Main -> Main` merely enqueues a Task
// that will set the event during the spin.
MOZ_HANDLEWATCHER_GTEST_FROM_TO(Main, Main);
MOZ_HANDLEWATCHER_GTEST_FROM_TO(Main, Side);
MOZ_HANDLEWATCHER_GTEST_FROM_TO(Main, Background);
MOZ_HANDLEWATCHER_GTEST_FROM_TO(Side, Main);
MOZ_HANDLEWATCHER_GTEST_FROM_TO(Side, Side);
MOZ_HANDLEWATCHER_GTEST_FROM_TO(Side, Background);
MOZ_HANDLEWATCHER_GTEST_FROM_TO(Background, Main);
MOZ_HANDLEWATCHER_GTEST_FROM_TO(Background, Side);
MOZ_HANDLEWATCHER_GTEST_FROM_TO(Background, Background);

///////////////////////////////////////////////////////////////////////
// Ad-hoc tests: reentrancy
//
// Test that HandleWatcher neither deadlocks nor loses data if its release of a
// referenced object causes the invocation of another method on HandleWatcher.

// Reentrancy case 1/2: the event target.
namespace {
class MockEventTarget final : public nsIEventTarget {
  NS_DECL_THREADSAFE_ISUPPORTS

 private:
  // Map from registered shutdown tasks to whether or not they have been (or are
  // being) executed. (This should probably guarantee some deterministic order,
  // and also be mutex-protected; but that doesn't matter here.)
  nsTHashMap<RefPtr<nsITargetShutdownTask>, bool> mShutdownTasks;
  // Out-of band task to be run last at destruction time, regardless of anything
  // else.
  std::function<void(void)> mDeathAction;

  ~MockEventTarget() {
    for (auto& task : mShutdownTasks) {
      task.SetData(true);
      task.GetKey()->TargetShutdown();
    }
    if (mDeathAction) {
      mDeathAction();
    }
  }

 public:
  // shutdown task handling
  NS_IMETHOD RegisterShutdownTask(nsITargetShutdownTask* task) override {
    mShutdownTasks.WithEntryHandle(task, [&](auto entry) {
      if (entry.HasEntry()) {
        MOZ_CRASH("attempted to double-register shutdown task");
      }
      entry.Insert(false);
    });
    return NS_OK;
  }
  NS_IMETHOD UnregisterShutdownTask(nsITargetShutdownTask* task) override {
    mozilla::Maybe<bool> res = mShutdownTasks.Extract(task);
    if (!res.isSome()) {
      MOZ_CRASH("attempted to unregister non-registered task");
    }
    if (res.value()) {
      MOZ_CRASH("attempted to unregister already-executed shutdown task");
    }
    return NS_OK;
  }
  void RegisterDeathAction(std::function<void(void)>&& f) {
    mDeathAction = std::move(f);
  }

  // other nsIEventTarget methods (that we don't actually use)
  NS_IMETHOD_(bool) IsOnCurrentThreadInfallible(void) { return false; }
  NS_IMETHOD IsOnCurrentThread(bool* _retval) {
    *_retval = false;
    return NS_OK;
  }
  NS_IMETHOD Dispatch(already_AddRefed<nsIRunnable>, uint32_t) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD DispatchFromScript(nsIRunnable*, uint32_t) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  NS_IMETHOD DelayedDispatch(already_AddRefed<nsIRunnable>, uint32_t) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
};
NS_IMPL_ISUPPORTS(MockEventTarget, nsIEventTarget)
}  // anonymous namespace

// Test that a HandleWatcher neither deadlocks nor loses data if it's invoked
// when it releases its target.
TEST_F(TestHandleWatcher, TargetDestructionRecurrency) {
  WindowsEventObject e1, e2;
  bool b1 = false, b2 = false;
  HandleWatcher hw;

  {
    RefPtr<MockEventTarget> p = mozilla::MakeRefPtr<MockEventTarget>();

    hw.Watch(e1.handle, p.get(), NS_NewRunnableFunction("first callback", [&] {
               b1 = true;
               PingMainThread();
             }));

    p->RegisterDeathAction([&] {
      hw.Watch(e2.handle, mozilla::GetMainThreadSerialEventTarget(),
               NS_NewRunnableFunction("second callback", [&] { b2 = true; }));
    });
  }

  ASSERT_FALSE(hw.IsStopped());
  hw.Stop();
  ASSERT_FALSE(hw.IsStopped());  // [sic]

  e1.Set();  // should do nothing
  e2.Set();
  ASSERT_TRUE(SpinEventLoopUntil([&] { return b1 || b2; }).ok());
  ASSERT_FALSE(b1);
  ASSERT_TRUE(b2);
}

// Reentrancy case 2/2: the runnable.
namespace {
class MockRunnable final : public nsIRunnable {
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_IMETHOD Run() override {
    MOZ_CRASH("MockRunnable was invoked");
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  std::function<void(void)> mDeathAction;

 public:
  void RegisterDeathAction(std::function<void(void)>&& f) {
    mDeathAction = std::move(f);
  }

 private:
  ~MockRunnable() {
    if (mDeathAction) {
      mDeathAction();
    }
  }
};
NS_IMPL_ISUPPORTS(MockRunnable, nsIRunnable)
}  // anonymous namespace

// Test that a HandleWatcher neither deadlocks nor loses data if it's invoked
// when it releases its task.
TEST_F(TestHandleWatcher, TaskDestructionRecurrency) {
  WindowsEventObject e1, e2;
  bool run = false;
  HandleWatcher hw;

  auto thread = SpawnNewBackgroundQueue();

  {
    RefPtr<MockRunnable> runnable = mozilla::MakeRefPtr<MockRunnable>();

    runnable->RegisterDeathAction([&] {
      hw.Watch(e2.handle, thread, NS_NewRunnableFunction("callback", [&] {
                 run = true;
                 PingMainThread();
               }));
    });

    hw.Watch(e1.handle, thread.get(), runnable.forget());
  }

  ASSERT_FALSE(hw.IsStopped());
  hw.Stop();
  ASSERT_FALSE(hw.IsStopped());  // [sic]

  e1.Set();
  // give MockRunnable a chance to run (and therefore crash) if it somehow
  // hasn't been discmnnected
  ASSERT_TRUE(
      SpinEventLoopUntil([&] { return false; }, Milliseconds(10)).timedOut());

  e2.Set();
  ASSERT_TRUE(SpinEventLoopUntil([&] { return run; }).ok());
  ASSERT_TRUE(run);
}
