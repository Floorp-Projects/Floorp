/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "base/message_loop.h"

#include "mozilla/TaskQueue.h"
#include "mozilla/MozPromise.h"
#include "mozilla/Unused.h"

#include "nsISupportsImpl.h"
#include "mozilla/SharedThreadPool.h"
#include "VideoUtils.h"

using namespace mozilla;

typedef MozPromise<int, double, false> TestPromise;
typedef TestPromise::ResolveOrRejectValue RRValue;

class MOZ_STACK_CLASS AutoTaskQueue
{
public:
  AutoTaskQueue()
    : mTaskQueue(new TaskQueue(GetMediaThreadPool(MediaThreadType::PLAYBACK)))
  {}

  ~AutoTaskQueue()
  {
    mTaskQueue->AwaitShutdownAndIdle();
  }

  TaskQueue* Queue() { return mTaskQueue; }
private:
  RefPtr<TaskQueue> mTaskQueue;
};

class DelayedResolveOrReject : public Runnable
{
public:
  DelayedResolveOrReject(TaskQueue* aTaskQueue,
                         TestPromise::Private* aPromise,
                         const TestPromise::ResolveOrRejectValue& aValue,
                         int aIterations)
    : mozilla::Runnable("DelayedResolveOrReject")
    , mTaskQueue(aTaskQueue)
    , mPromise(aPromise)
    , mValue(aValue)
    , mIterations(aIterations)
  {}

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
    if (!mPromise) {
      // Canceled.
      return NS_OK;
    }

    if (--mIterations == 0) {
      mPromise->ResolveOrReject(mValue, __func__);
      return NS_OK;
    }

    nsCOMPtr<nsIRunnable> r = this;
    return mTaskQueue->Dispatch(r.forget());
  }

  void Cancel() {
    mPromise = nullptr;
  }

protected:
  ~DelayedResolveOrReject() {}

private:
  RefPtr<TaskQueue> mTaskQueue;
  RefPtr<TestPromise::Private> mPromise;
  TestPromise::ResolveOrRejectValue mValue;
  int mIterations;
};

template<typename FunctionType>
void
RunOnTaskQueue(TaskQueue* aQueue, FunctionType aFun)
{
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction("RunOnTaskQueue", aFun);
  Unused << aQueue->Dispatch(r.forget());
}

// std::function can't come soon enough. :-(
#define DO_FAIL []() { EXPECT_TRUE(false); return TestPromise::CreateAndReject(0, __func__); }

TEST(MozPromise, BasicResolve)
{
  AutoTaskQueue atq;
  RefPtr<TaskQueue> queue = atq.Queue();
  RunOnTaskQueue(queue, [queue] () -> void {
    TestPromise::CreateAndResolve(42, __func__)->Then(queue, __func__,
      [queue] (int aResolveValue) -> void { EXPECT_EQ(aResolveValue, 42); queue->BeginShutdown(); },
      DO_FAIL);
  });
}

TEST(MozPromise, BasicReject)
{
  AutoTaskQueue atq;
  RefPtr<TaskQueue> queue = atq.Queue();
  RunOnTaskQueue(queue, [queue] () -> void {
    TestPromise::CreateAndReject(42.0, __func__)->Then(queue, __func__,
      DO_FAIL,
      [queue] (int aRejectValue) -> void { EXPECT_EQ(aRejectValue, 42.0); queue->BeginShutdown(); });
  });
}

TEST(MozPromise, BasicResolveOrRejectResolved)
{
  AutoTaskQueue atq;
  RefPtr<TaskQueue> queue = atq.Queue();
  RunOnTaskQueue(queue, [queue] () -> void {
    TestPromise::CreateAndResolve(42, __func__)->Then(queue, __func__,
      [queue] (const TestPromise::ResolveOrRejectValue& aValue) -> void
      {
        EXPECT_TRUE(aValue.IsResolve());
        EXPECT_FALSE(aValue.IsReject());
        EXPECT_FALSE(aValue.IsNothing());
        EXPECT_EQ(aValue.ResolveValue(), 42);
        queue->BeginShutdown();
      });
  });
}

TEST(MozPromise, BasicResolveOrRejectRejected)
{
  AutoTaskQueue atq;
  RefPtr<TaskQueue> queue = atq.Queue();
  RunOnTaskQueue(queue, [queue] () -> void {
    TestPromise::CreateAndReject(42.0, __func__)->Then(queue, __func__,
      [queue] (const TestPromise::ResolveOrRejectValue& aValue) -> void
      {
        EXPECT_TRUE(aValue.IsReject());
        EXPECT_FALSE(aValue.IsResolve());
        EXPECT_FALSE(aValue.IsNothing());
        EXPECT_EQ(aValue.RejectValue(), 42.0);
        queue->BeginShutdown();
      });
  });
}

TEST(MozPromise, AsyncResolve)
{
  AutoTaskQueue atq;
  RefPtr<TaskQueue> queue = atq.Queue();
  RunOnTaskQueue(queue, [queue] () -> void {
    RefPtr<TestPromise::Private> p = new TestPromise::Private(__func__);

    // Kick off three racing tasks, and make sure we get the one that finishes earliest.
    RefPtr<DelayedResolveOrReject> a = new DelayedResolveOrReject(queue, p, RRValue::MakeResolve(32), 10);
    RefPtr<DelayedResolveOrReject> b = new DelayedResolveOrReject(queue, p, RRValue::MakeResolve(42), 5);
    RefPtr<DelayedResolveOrReject> c = new DelayedResolveOrReject(queue, p, RRValue::MakeReject(32.0), 7);

    nsCOMPtr<nsIRunnable> ref = a.get();
    Unused << queue->Dispatch(ref.forget());
    ref = b.get();
    Unused << queue->Dispatch(ref.forget());
    ref = c.get();
    Unused << queue->Dispatch(ref.forget());

    p->Then(queue, __func__, [queue, a, b, c] (int aResolveValue) -> void {
      EXPECT_EQ(aResolveValue, 42);
      a->Cancel();
      b->Cancel();
      c->Cancel();
      queue->BeginShutdown();
    }, DO_FAIL);
  });
}

TEST(MozPromise, CompletionPromises)
{
  bool invokedPass = false;
  AutoTaskQueue atq;
  RefPtr<TaskQueue> queue = atq.Queue();
  RunOnTaskQueue(queue, [queue, &invokedPass] () -> void {
    TestPromise::CreateAndResolve(40, __func__)
    ->Then(queue, __func__,
      [] (int aVal) -> RefPtr<TestPromise> { return TestPromise::CreateAndResolve(aVal + 10, __func__); },
      DO_FAIL)
    ->Then(queue, __func__,
           [&invokedPass] (int aVal) {
             invokedPass = true;
             return TestPromise::CreateAndResolve(aVal, __func__);
           }, DO_FAIL)
    ->Then(queue, __func__,
      [queue] (int aVal) -> RefPtr<TestPromise> {
        RefPtr<TestPromise::Private> p = new TestPromise::Private(__func__);
        nsCOMPtr<nsIRunnable> resolver = new DelayedResolveOrReject(queue, p, RRValue::MakeResolve(aVal - 8), 10);
        Unused << queue->Dispatch(resolver.forget());
        return RefPtr<TestPromise>(p);
      },
      DO_FAIL)
    ->Then(queue, __func__,
      [] (int aVal) -> RefPtr<TestPromise> { return TestPromise::CreateAndReject(double(aVal - 42) + 42.0, __func__); },
      DO_FAIL)
    ->Then(queue, __func__,
      DO_FAIL,
      [queue, &invokedPass] (double aVal) -> void { EXPECT_EQ(aVal, 42.0); EXPECT_TRUE(invokedPass); queue->BeginShutdown(); });
  });
}

TEST(MozPromise, PromiseAllResolve)
{
  AutoTaskQueue atq;
  RefPtr<TaskQueue> queue = atq.Queue();
  RunOnTaskQueue(queue, [queue] () -> void {

    nsTArray<RefPtr<TestPromise>> promises;
    promises.AppendElement(TestPromise::CreateAndResolve(22, __func__));
    promises.AppendElement(TestPromise::CreateAndResolve(32, __func__));
    promises.AppendElement(TestPromise::CreateAndResolve(42, __func__));

    TestPromise::All(queue, promises)->Then(queue, __func__,
      [queue] (const nsTArray<int>& aResolveValues) -> void {
        EXPECT_EQ(aResolveValues.Length(), 3UL);
        EXPECT_EQ(aResolveValues[0], 22);
        EXPECT_EQ(aResolveValues[1], 32);
        EXPECT_EQ(aResolveValues[2], 42);
        queue->BeginShutdown();
      },
      []() { EXPECT_TRUE(false); }
    );
  });
}

TEST(MozPromise, PromiseAllReject)
{
  AutoTaskQueue atq;
  RefPtr<TaskQueue> queue = atq.Queue();
  RunOnTaskQueue(queue, [queue] () -> void {

    nsTArray<RefPtr<TestPromise>> promises;
    promises.AppendElement(TestPromise::CreateAndResolve(22, __func__));
    promises.AppendElement(TestPromise::CreateAndReject(32.0, __func__));
    promises.AppendElement(TestPromise::CreateAndResolve(42, __func__));
   // Ensure that more than one rejection doesn't cause a crash (bug #1207312)
    promises.AppendElement(TestPromise::CreateAndReject(52.0, __func__));

    TestPromise::All(queue, promises)->Then(queue, __func__,
      []() { EXPECT_TRUE(false); },
      [queue] (float aRejectValue) -> void {
        EXPECT_EQ(aRejectValue, 32.0);
        queue->BeginShutdown();
      }
    );
  });
}

// Test we don't hit the assertions in MozPromise when exercising promise
// chaining upon task queue shutdown.
TEST(MozPromise, Chaining)
{
  // We declare this variable before |atq| to ensure
  // the destructor is run after |holder.Disconnect()|.
  MozPromiseRequestHolder<TestPromise> holder;

  AutoTaskQueue atq;
  RefPtr<TaskQueue> queue = atq.Queue();

  RunOnTaskQueue(queue, [queue, &holder] () {
    auto p = TestPromise::CreateAndResolve(42, __func__);
    const size_t kIterations = 100;
    for (size_t i = 0; i < kIterations; ++i) {
      p = p->Then(queue, __func__,
        [] (int aVal) {
          EXPECT_EQ(aVal, 42);
          return TestPromise::CreateAndResolve(aVal, __func__);
        },
        [] (double aVal) {
          return TestPromise::CreateAndReject(aVal, __func__);
        }
      );

      if (i == kIterations / 2) {
        p->Then(queue, __func__,
          [queue, &holder] () {
            holder.Disconnect();
            queue->BeginShutdown();
          },
          DO_FAIL);
      }
    }
    // We will hit the assertion if we don't disconnect the leaf Request
    // in the promise chain.
    p->Then(queue, __func__, [] () {}, [] () {})->Track(holder);
  });
}

TEST(MozPromise, ResolveOrRejectValue)
{
  using MyPromise = MozPromise<UniquePtr<int>, bool, false>;
  using RRValue = MyPromise::ResolveOrRejectValue;

  RRValue val;
  EXPECT_TRUE(val.IsNothing());
  EXPECT_FALSE(val.IsResolve());
  EXPECT_FALSE(val.IsReject());

  val.SetResolve(MakeUnique<int>(87));
  EXPECT_FALSE(val.IsNothing());
  EXPECT_TRUE(val.IsResolve());
  EXPECT_FALSE(val.IsReject());
  EXPECT_EQ(87, *val.ResolveValue());

  // IsResolve() should remain true after std::move().
  UniquePtr<int> i = std::move(val.ResolveValue());
  EXPECT_EQ(87, *i);
  EXPECT_TRUE(val.IsResolve());
  EXPECT_EQ(val.ResolveValue().get(), nullptr);
}

TEST(MozPromise, MoveOnlyType)
{
  using MyPromise = MozPromise<UniquePtr<int>, bool, true>;
  using RRValue = MyPromise::ResolveOrRejectValue;

  AutoTaskQueue atq;
  RefPtr<TaskQueue> queue = atq.Queue();

  MyPromise::CreateAndResolve(MakeUnique<int>(87), __func__)
  ->Then(queue, __func__,
    [](UniquePtr<int> aVal) {
      EXPECT_EQ(87, *aVal);
    },
    []() { EXPECT_TRUE(false); });

  MyPromise::CreateAndResolve(MakeUnique<int>(87), __func__)
  ->Then(queue, __func__,
    [queue](RRValue&& aVal) {
      EXPECT_FALSE(aVal.IsNothing());
      EXPECT_TRUE(aVal.IsResolve());
      EXPECT_FALSE(aVal.IsReject());
      EXPECT_EQ(87, *aVal.ResolveValue());

      // std::move() shouldn't change the resolve/reject state of aVal.
      RRValue val = std::move(aVal);
      EXPECT_TRUE(aVal.IsResolve());
      EXPECT_EQ(nullptr, aVal.ResolveValue().get());
      EXPECT_EQ(87, *val.ResolveValue());

      queue->BeginShutdown();
    });
}

TEST(MozPromise, HeterogeneousChaining)
{
  using Promise1 = MozPromise<UniquePtr<char>, bool, true>;
  using Promise2 = MozPromise<UniquePtr<int>, bool, true>;
  using RRValue1 = Promise1::ResolveOrRejectValue;
  using RRValue2 = Promise2::ResolveOrRejectValue;

  MozPromiseRequestHolder<Promise2> holder;

  AutoTaskQueue atq;
  RefPtr<TaskQueue> queue = atq.Queue();

  RunOnTaskQueue(queue, [queue, &holder]() {
    Promise1::CreateAndResolve(MakeUnique<char>(0), __func__)
      ->Then(queue,
             __func__,
             [&holder]() {
               holder.Disconnect();
               return Promise2::CreateAndResolve(MakeUnique<int>(0), __func__);
             })
      ->Then(queue,
             __func__,
             []() {
               // Shouldn't be called for we've disconnected the request.
               EXPECT_FALSE(true);
             })
      ->Track(holder);
  });

  Promise1::CreateAndResolve(MakeUnique<char>(87), __func__)
    ->Then(queue,
           __func__,
           [](UniquePtr<char> aVal) {
             EXPECT_EQ(87, *aVal);
             return Promise2::CreateAndResolve(MakeUnique<int>(94), __func__);
           },
           []() {
             return Promise2::CreateAndResolve(MakeUnique<int>(95), __func__);
           })
    ->Then(queue,
           __func__,
           [](UniquePtr<int> aVal) { EXPECT_EQ(94, *aVal); },
           []() { EXPECT_FALSE(true); });

  Promise1::CreateAndResolve(MakeUnique<char>(87), __func__)
    ->Then(queue,
           __func__,
           [](RRValue1&& aVal) {
             EXPECT_EQ(87, *aVal.ResolveValue());
             return Promise2::CreateAndResolve(MakeUnique<int>(94), __func__);
           })
    ->Then(queue, __func__, [queue](RRValue2&& aVal) {
      EXPECT_EQ(94, *aVal.ResolveValue());
      queue->BeginShutdown();
    });
}

TEST(MozPromise, XPCOMEventTarget)
{
  TestPromise::CreateAndResolve(42, __func__)->Then(GetCurrentThreadSerialEventTarget(), __func__,
    [] (int aResolveValue) -> void { EXPECT_EQ(aResolveValue, 42); },
    DO_FAIL);

  // Spin the event loop.
  NS_ProcessPendingEvents(nullptr);
}

TEST(MozPromise, MessageLoopEventTarget)
{
  TestPromise::CreateAndResolve(42, __func__)->Then(MessageLoop::current()->SerialEventTarget(), __func__,
    [] (int aResolveValue) -> void { EXPECT_EQ(aResolveValue, 42); },
    DO_FAIL);

  // Spin the event loop.
  NS_ProcessPendingEvents(nullptr);
}

#undef DO_FAIL
