/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VideoUtils.h"
#include "base/message_loop.h"
#include "gtest/gtest.h"
#include "mozilla/ChaosMode.h"
#include "mozilla/MozPromise.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/Unused.h"
#include "nsISupportsImpl.h"

using namespace mozilla;

typedef MozPromise<int, double, false> TestPromise;
typedef MozPromise<int, double, true /* exclusive */> TestPromiseExcl;
typedef TestPromise::ResolveOrRejectValue RRValue;

class MOZ_STACK_CLASS AutoTaskQueue {
 public:
  AutoTaskQueue()
      : mTaskQueue(
            TaskQueue::Create(GetMediaThreadPool(MediaThreadType::SUPERVISOR),
                              "TestMozPromise AutoTaskQueue")) {}

  ~AutoTaskQueue() { mTaskQueue->AwaitShutdownAndIdle(); }

  TaskQueue* Queue() { return mTaskQueue; }

 private:
  RefPtr<TaskQueue> mTaskQueue;
};

class DelayedResolveOrReject : public Runnable {
 public:
  DelayedResolveOrReject(TaskQueue* aTaskQueue, TestPromise::Private* aPromise,
                         const TestPromise::ResolveOrRejectValue& aValue,
                         int aIterations)
      : mozilla::Runnable("DelayedResolveOrReject"),
        mTaskQueue(aTaskQueue),
        mPromise(aPromise),
        mValue(aValue),
        mIterations(aIterations) {}

  NS_IMETHOD Run() override {
    MOZ_RELEASE_ASSERT(mTaskQueue->IsCurrentThreadIn());
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

  void Cancel() { mPromise = nullptr; }

 protected:
  ~DelayedResolveOrReject() = default;

 private:
  RefPtr<TaskQueue> mTaskQueue;
  RefPtr<TestPromise::Private> mPromise;
  TestPromise::ResolveOrRejectValue mValue;
  int mIterations;
};

struct DtorTracker {
  DtorTracker(nsTArray<size_t>& aList, size_t aId) : mList(aList), mId(aId) {}

  DtorTracker(DtorTracker&& aOther) = delete;
  DtorTracker& operator=(DtorTracker&&) = delete;

  ~DtorTracker() { mList.AppendElement(mId); }

  nsTArray<size_t>& mList;
  const size_t mId;
};

template <typename FunctionType>
void RunOnTaskQueue(TaskQueue* aQueue, FunctionType aFun) {
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction("RunOnTaskQueue", aFun);
  Unused << aQueue->Dispatch(r.forget());
}

// std::function can't come soon enough. :-(
#define DO_FAIL                                       \
  []() {                                              \
    EXPECT_TRUE(false);                               \
    return TestPromise::CreateAndReject(0, __func__); \
  }

TEST(MozPromise, BasicResolve)
{
  AutoTaskQueue atq;
  RefPtr<TaskQueue> queue = atq.Queue();
  RunOnTaskQueue(queue, [queue]() -> void {
    TestPromise::CreateAndResolve(42, __func__)
        ->Then(
            queue, __func__,
            [queue](int aResolveValue) -> void {
              EXPECT_EQ(aResolveValue, 42);
              queue->BeginShutdown();
            },
            DO_FAIL);
  });
}

TEST(MozPromise, BasicReject)
{
  AutoTaskQueue atq;
  RefPtr<TaskQueue> queue = atq.Queue();
  RunOnTaskQueue(queue, [queue]() -> void {
    TestPromise::CreateAndReject(42.0, __func__)
        ->Then(queue, __func__, DO_FAIL, [queue](int aRejectValue) -> void {
          EXPECT_EQ(aRejectValue, 42.0);
          queue->BeginShutdown();
        });
  });
}

TEST(MozPromise, BasicResolveOrRejectResolved)
{
  AutoTaskQueue atq;
  RefPtr<TaskQueue> queue = atq.Queue();
  RunOnTaskQueue(queue, [queue]() -> void {
    TestPromise::CreateAndResolve(42, __func__)
        ->Then(
            queue, __func__,
            [queue](const TestPromise::ResolveOrRejectValue& aValue) -> void {
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
  RunOnTaskQueue(queue, [queue]() -> void {
    TestPromise::CreateAndReject(42.0, __func__)
        ->Then(
            queue, __func__,
            [queue](const TestPromise::ResolveOrRejectValue& aValue) -> void {
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
  RunOnTaskQueue(queue, [queue]() -> void {
    RefPtr<TestPromise::Private> p = new TestPromise::Private(__func__);

    // Kick off three racing tasks, and make sure we get the one that finishes
    // earliest.
    RefPtr<DelayedResolveOrReject> a =
        new DelayedResolveOrReject(queue, p, RRValue::MakeResolve(32), 10);
    RefPtr<DelayedResolveOrReject> b =
        new DelayedResolveOrReject(queue, p, RRValue::MakeResolve(42), 5);
    RefPtr<DelayedResolveOrReject> c =
        new DelayedResolveOrReject(queue, p, RRValue::MakeReject(32.0), 7);

    nsCOMPtr<nsIRunnable> ref = a.get();
    Unused << queue->Dispatch(ref.forget());
    ref = b.get();
    Unused << queue->Dispatch(ref.forget());
    ref = c.get();
    Unused << queue->Dispatch(ref.forget());

    p->Then(
        queue, __func__,
        [queue, a, b, c](int aResolveValue) -> void {
          EXPECT_EQ(aResolveValue, 42);
          a->Cancel();
          b->Cancel();
          c->Cancel();
          queue->BeginShutdown();
        },
        DO_FAIL);
  });
}

TEST(MozPromise, CompletionPromises)
{
  bool invokedPass = false;
  AutoTaskQueue atq;
  RefPtr<TaskQueue> queue = atq.Queue();
  RunOnTaskQueue(queue, [queue, &invokedPass]() -> void {
    TestPromise::CreateAndResolve(40, __func__)
        ->Then(
            queue, __func__,
            [](int aVal) -> RefPtr<TestPromise> {
              return TestPromise::CreateAndResolve(aVal + 10, __func__);
            },
            DO_FAIL)
        ->Then(
            queue, __func__,
            [&invokedPass](int aVal) {
              invokedPass = true;
              return TestPromise::CreateAndResolve(aVal, __func__);
            },
            DO_FAIL)
        ->Then(
            queue, __func__,
            [queue](int aVal) -> RefPtr<TestPromise> {
              RefPtr<TestPromise::Private> p =
                  new TestPromise::Private(__func__);
              nsCOMPtr<nsIRunnable> resolver = new DelayedResolveOrReject(
                  queue, p, RRValue::MakeResolve(aVal - 8), 10);
              Unused << queue->Dispatch(resolver.forget());
              return RefPtr<TestPromise>(p);
            },
            DO_FAIL)
        ->Then(
            queue, __func__,
            [](int aVal) -> RefPtr<TestPromise> {
              return TestPromise::CreateAndReject(double(aVal - 42) + 42.0,
                                                  __func__);
            },
            DO_FAIL)
        ->Then(queue, __func__, DO_FAIL,
               [queue, &invokedPass](double aVal) -> void {
                 EXPECT_EQ(aVal, 42.0);
                 EXPECT_TRUE(invokedPass);
                 queue->BeginShutdown();
               });
  });
}

TEST(MozPromise, PromiseAllResolve)
{
  AutoTaskQueue atq;
  RefPtr<TaskQueue> queue = atq.Queue();
  RunOnTaskQueue(queue, [queue]() -> void {
    nsTArray<RefPtr<TestPromise>> promises;
    promises.AppendElement(TestPromise::CreateAndResolve(22, __func__));
    promises.AppendElement(TestPromise::CreateAndResolve(32, __func__));
    promises.AppendElement(TestPromise::CreateAndResolve(42, __func__));

    TestPromise::All(queue, promises)
        ->Then(
            queue, __func__,
            [queue](const CopyableTArray<int>& aResolveValues) -> void {
              EXPECT_EQ(aResolveValues.Length(), 3UL);
              EXPECT_EQ(aResolveValues[0], 22);
              EXPECT_EQ(aResolveValues[1], 32);
              EXPECT_EQ(aResolveValues[2], 42);
              queue->BeginShutdown();
            },
            []() { EXPECT_TRUE(false); });
  });
}

TEST(MozPromise, PromiseAllResolveAsync)
{
  AutoTaskQueue atq;
  RefPtr<TaskQueue> queue = atq.Queue();
  RunOnTaskQueue(queue, [queue]() -> void {
    nsTArray<RefPtr<TestPromise>> promises;
    promises.AppendElement(InvokeAsync(queue, __func__, []() {
      return TestPromise::CreateAndResolve(22, __func__);
    }));
    promises.AppendElement(InvokeAsync(queue, __func__, []() {
      return TestPromise::CreateAndResolve(32, __func__);
    }));
    promises.AppendElement(InvokeAsync(queue, __func__, []() {
      return TestPromise::CreateAndResolve(42, __func__);
    }));

    TestPromise::All(queue, promises)
        ->Then(
            queue, __func__,
            [queue](const CopyableTArray<int>& aResolveValues) -> void {
              EXPECT_EQ(aResolveValues.Length(), 3UL);
              EXPECT_EQ(aResolveValues[0], 22);
              EXPECT_EQ(aResolveValues[1], 32);
              EXPECT_EQ(aResolveValues[2], 42);
              queue->BeginShutdown();
            },
            []() { EXPECT_TRUE(false); });
  });
}

TEST(MozPromise, PromiseAllReject)
{
  AutoTaskQueue atq;
  RefPtr<TaskQueue> queue = atq.Queue();
  RunOnTaskQueue(queue, [queue]() -> void {
    nsTArray<RefPtr<TestPromise>> promises;
    promises.AppendElement(TestPromise::CreateAndResolve(22, __func__));
    promises.AppendElement(TestPromise::CreateAndReject(32.0, __func__));
    promises.AppendElement(TestPromise::CreateAndResolve(42, __func__));
    // Ensure that more than one rejection doesn't cause a crash (bug #1207312)
    promises.AppendElement(TestPromise::CreateAndReject(52.0, __func__));

    TestPromise::All(queue, promises)
        ->Then(
            queue, __func__, []() { EXPECT_TRUE(false); },
            [queue](float aRejectValue) -> void {
              EXPECT_EQ(aRejectValue, 32.0);
              queue->BeginShutdown();
            });
  });
}

TEST(MozPromise, PromiseAllRejectAsync)
{
  AutoTaskQueue atq;
  RefPtr<TaskQueue> queue = atq.Queue();
  RunOnTaskQueue(queue, [queue]() -> void {
    nsTArray<RefPtr<TestPromise>> promises;
    promises.AppendElement(InvokeAsync(queue, __func__, []() {
      return TestPromise::CreateAndResolve(22, __func__);
    }));
    promises.AppendElement(InvokeAsync(queue, __func__, []() {
      return TestPromise::CreateAndReject(32.0, __func__);
    }));
    promises.AppendElement(InvokeAsync(queue, __func__, []() {
      return TestPromise::CreateAndResolve(42, __func__);
    }));
    // Ensure that more than one rejection doesn't cause a crash (bug #1207312)
    promises.AppendElement(InvokeAsync(queue, __func__, []() {
      return TestPromise::CreateAndReject(52.0, __func__);
    }));

    TestPromise::All(queue, promises)
        ->Then(
            queue, __func__, []() { EXPECT_TRUE(false); },
            [queue](float aRejectValue) -> void {
              EXPECT_EQ(aRejectValue, 32.0);
              queue->BeginShutdown();
            });
  });
}

TEST(MozPromise, PromiseAllSettled)
{
  AutoTaskQueue atq;
  RefPtr<TaskQueue> queue = atq.Queue();
  RunOnTaskQueue(queue, [queue]() -> void {
    nsTArray<RefPtr<TestPromise>> promises;
    promises.AppendElement(TestPromise::CreateAndResolve(22, __func__));
    promises.AppendElement(TestPromise::CreateAndReject(32.0, __func__));
    promises.AppendElement(TestPromise::CreateAndResolve(42, __func__));
    promises.AppendElement(TestPromise::CreateAndReject(52.0, __func__));

    TestPromise::AllSettled(queue, promises)
        ->Then(
            queue, __func__,
            [queue](const TestPromise::AllSettledPromiseType::ResolveValueType&
                        aResolveValues) -> void {
              EXPECT_EQ(aResolveValues.Length(), 4UL);
              EXPECT_TRUE(aResolveValues[0].IsResolve());
              EXPECT_EQ(aResolveValues[0].ResolveValue(), 22);
              EXPECT_FALSE(aResolveValues[1].IsResolve());
              EXPECT_EQ(aResolveValues[1].RejectValue(), 32.0);
              EXPECT_TRUE(aResolveValues[2].IsResolve());
              EXPECT_EQ(aResolveValues[2].ResolveValue(), 42);
              EXPECT_FALSE(aResolveValues[3].IsResolve());
              EXPECT_EQ(aResolveValues[3].RejectValue(), 52.0);
              queue->BeginShutdown();
            },
            []() { EXPECT_TRUE(false); });
  });
}

TEST(MozPromise, PromiseAllSettledAsync)
{
  AutoTaskQueue atq;
  RefPtr<TaskQueue> queue = atq.Queue();

  RunOnTaskQueue(queue, [queue]() -> void {
    nsTArray<RefPtr<TestPromise>> promises;
    promises.AppendElement(InvokeAsync(queue, __func__, []() {
      return TestPromise::CreateAndResolve(22, __func__);
    }));
    promises.AppendElement(InvokeAsync(queue, __func__, []() {
      return TestPromise::CreateAndReject(32.0, __func__);
    }));
    promises.AppendElement(InvokeAsync(queue, __func__, []() {
      return TestPromise::CreateAndResolve(42, __func__);
    }));
    promises.AppendElement(InvokeAsync(queue, __func__, []() {
      return TestPromise::CreateAndReject(52.0, __func__);
    }));

    TestPromise::AllSettled(queue, promises)
        ->Then(
            queue, __func__,
            [queue](const TestPromise::AllSettledPromiseType::ResolveValueType&
                        aResolveValues) -> void {
              EXPECT_EQ(aResolveValues.Length(), 4UL);
              EXPECT_TRUE(aResolveValues[0].IsResolve());
              EXPECT_EQ(aResolveValues[0].ResolveValue(), 22);
              EXPECT_FALSE(aResolveValues[1].IsResolve());
              EXPECT_EQ(aResolveValues[1].RejectValue(), 32.0);
              EXPECT_TRUE(aResolveValues[2].IsResolve());
              EXPECT_EQ(aResolveValues[2].ResolveValue(), 42);
              EXPECT_FALSE(aResolveValues[3].IsResolve());
              EXPECT_EQ(aResolveValues[3].RejectValue(), 52.0);
              queue->BeginShutdown();
            },
            []() { EXPECT_TRUE(false); });
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

  RunOnTaskQueue(queue, [queue, &holder]() {
    auto p = TestPromise::CreateAndResolve(42, __func__);
    const size_t kIterations = 100;
    for (size_t i = 0; i < kIterations; ++i) {
      p = p->Then(
          queue, __func__,
          [](int aVal) {
            EXPECT_EQ(aVal, 42);
            return TestPromise::CreateAndResolve(aVal, __func__);
          },
          [](double aVal) {
            return TestPromise::CreateAndReject(aVal, __func__);
          });

      if (i == kIterations / 2) {
        p->Then(
            queue, __func__,
            [queue, &holder]() {
              holder.Disconnect();
              queue->BeginShutdown();
            },
            DO_FAIL);
      }
    }
    // We will hit the assertion if we don't disconnect the leaf Request
    // in the promise chain.
    p->Then(
         queue, __func__, []() {}, []() {})
        ->Track(holder);
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
      ->Then(
          queue, __func__, [](UniquePtr<int> aVal) { EXPECT_EQ(87, *aVal); },
          []() { EXPECT_TRUE(false); });

  MyPromise::CreateAndResolve(MakeUnique<int>(87), __func__)
      ->Then(queue, __func__, [queue](RRValue&& aVal) {
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
        ->Then(queue, __func__,
               [&holder]() {
                 holder.Disconnect();
                 return Promise2::CreateAndResolve(MakeUnique<int>(0),
                                                   __func__);
               })
        ->Then(queue, __func__,
               []() {
                 // Shouldn't be called for we've disconnected the request.
                 EXPECT_FALSE(true);
               })
        ->Track(holder);
  });

  Promise1::CreateAndResolve(MakeUnique<char>(87), __func__)
      ->Then(
          queue, __func__,
          [](UniquePtr<char> aVal) {
            EXPECT_EQ(87, *aVal);
            return Promise2::CreateAndResolve(MakeUnique<int>(94), __func__);
          },
          []() {
            return Promise2::CreateAndResolve(MakeUnique<int>(95), __func__);
          })
      ->Then(
          queue, __func__, [](UniquePtr<int> aVal) { EXPECT_EQ(94, *aVal); },
          []() { EXPECT_FALSE(true); });

  Promise1::CreateAndResolve(MakeUnique<char>(87), __func__)
      ->Then(queue, __func__,
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
  TestPromise::CreateAndResolve(42, __func__)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [](int aResolveValue) -> void { EXPECT_EQ(aResolveValue, 42); },
          DO_FAIL);

  // Spin the event loop.
  NS_ProcessPendingEvents(nullptr);
}

TEST(MozPromise, MessageLoopEventTarget)
{
  TestPromise::CreateAndResolve(42, __func__)
      ->Then(
          MessageLoop::current()->SerialEventTarget(), __func__,
          [](int aResolveValue) -> void { EXPECT_EQ(aResolveValue, 42); },
          DO_FAIL);

  // Spin the event loop.
  NS_ProcessPendingEvents(nullptr);
}

TEST(MozPromise, ChainTo)
{
  RefPtr<TestPromise> promise1 = TestPromise::CreateAndResolve(42, __func__);
  RefPtr<TestPromise::Private> promise2 = new TestPromise::Private(__func__);
  promise2->Then(
      GetCurrentSerialEventTarget(), __func__,
      [&](int aResolveValue) -> void { EXPECT_EQ(aResolveValue, 42); },
      DO_FAIL);

  promise1->ChainTo(promise2.forget(), __func__);

  // Spin the event loop.
  NS_ProcessPendingEvents(nullptr);
}

TEST(MozPromise, SynchronousTaskDispatch1)
{
  bool value = false;
  RefPtr<TestPromiseExcl::Private> promise =
      new TestPromiseExcl::Private(__func__);
  promise->UseSynchronousTaskDispatch(__func__);
  promise->Resolve(42, __func__);
  EXPECT_EQ(value, false);
  promise->Then(
      GetCurrentSerialEventTarget(), __func__,
      [&](int aResolveValue) -> void {
        EXPECT_EQ(aResolveValue, 42);
        value = true;
      },
      DO_FAIL);
  EXPECT_EQ(value, true);
}

TEST(MozPromise, SynchronousTaskDispatch2)
{
  bool value = false;
  RefPtr<TestPromiseExcl::Private> promise =
      new TestPromiseExcl::Private(__func__);
  promise->UseSynchronousTaskDispatch(__func__);
  promise->Then(
      GetCurrentSerialEventTarget(), __func__,
      [&](int aResolveValue) -> void {
        EXPECT_EQ(aResolveValue, 42);
        value = true;
      },
      DO_FAIL);
  EXPECT_EQ(value, false);
  promise->Resolve(42, __func__);
  EXPECT_EQ(value, true);
}

TEST(MozPromise, DirectTaskDispatch)
{
  bool value1 = false;
  bool value2 = false;

  // For direct task dispatch to be working, we must be within a
  // nested event loop. So the test itself must be dispatched within
  // a task.
  GetCurrentSerialEventTarget()->Dispatch(NS_NewRunnableFunction("test", [&]() {
    GetCurrentSerialEventTarget()->Dispatch(
        NS_NewRunnableFunction("test", [&]() {
          EXPECT_EQ(value1, true);
          value2 = true;
        }));

    RefPtr<TestPromise::Private> promise = new TestPromise::Private(__func__);
    promise->UseDirectTaskDispatch(__func__);
    promise->Resolve(42, __func__);
    EXPECT_EQ(value1, false);
    promise->Then(
        GetCurrentSerialEventTarget(), __func__,
        [&](int aResolveValue) -> void {
          EXPECT_EQ(aResolveValue, 42);
          EXPECT_EQ(value2, false);
          value1 = true;
        },
        DO_FAIL);
    EXPECT_EQ(value1, false);
  }));

  // Spin the event loop.
  NS_ProcessPendingEvents(nullptr);
}

TEST(MozPromise, ChainedDirectTaskDispatch)
{
  bool value1 = false;
  bool value2 = false;

  // For direct task dispatch to be working, we must be within a
  // nested event loop. So the test itself must be dispatched within
  // a task.
  GetCurrentSerialEventTarget()->Dispatch(NS_NewRunnableFunction("test", [&]() {
    GetCurrentSerialEventTarget()->Dispatch(
        NS_NewRunnableFunction("test", [&]() {
          EXPECT_EQ(value1, true);
          value2 = true;
        }));

    RefPtr<TestPromise::Private> promise1 = new TestPromise::Private(__func__);
    promise1->UseDirectTaskDispatch(__func__);
    promise1->Resolve(42, __func__);
    EXPECT_EQ(value1, false);
    promise1
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [&](int aResolveValue) -> RefPtr<TestPromise> {
              EXPECT_EQ(aResolveValue, 42);
              EXPECT_EQ(value2, false);
              RefPtr<TestPromise::Private> promise2 =
                  new TestPromise::Private(__func__);
              promise2->UseDirectTaskDispatch(__func__);
              promise2->Resolve(43, __func__);
              return promise2;
            },
            DO_FAIL)
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [&](int aResolveValue) -> void {
              EXPECT_EQ(aResolveValue, 43);
              EXPECT_EQ(value2, false);
              value1 = true;
            },
            DO_FAIL);
    EXPECT_EQ(value1, false);
  }));

  // Spin the event loop.
  NS_ProcessPendingEvents(nullptr);
}

TEST(MozPromise, ChainToDirectTaskDispatch)
{
  bool value1 = false;
  bool value2 = false;

  // For direct task dispatch to be working, we must be within a
  // nested event loop. So the test itself must be dispatched within
  // a task.
  GetCurrentSerialEventTarget()->Dispatch(NS_NewRunnableFunction("test", [&]() {
    GetCurrentSerialEventTarget()->Dispatch(
        NS_NewRunnableFunction("test", [&]() {
          EXPECT_EQ(value1, true);
          value2 = true;
        }));

    RefPtr<TestPromise::Private> promise1 = new TestPromise::Private(__func__);
    promise1->UseDirectTaskDispatch(__func__);

    RefPtr<TestPromise::Private> promise2 = new TestPromise::Private(__func__);
    promise2->Then(
        GetCurrentSerialEventTarget(), __func__,
        [&](int aResolveValue) -> void {
          EXPECT_EQ(aResolveValue, 42);
          EXPECT_EQ(value2, false);
          value1 = true;
        },
        DO_FAIL);

    promise1->ChainTo(promise2.forget(), __func__);
    EXPECT_EQ(value1, false);
    promise1->Resolve(42, __func__);
  }));

  // Spin the event loop.
  NS_ProcessPendingEvents(nullptr);
}

TEST(MozPromise, Map)
{
  int value = 0;
  bool ran_err = false;

  InvokeAsync(GetCurrentSerialEventTarget(), "test",
              [&]() { return TestPromise::CreateAndResolve(18, "test"); })
      ->Map(GetCurrentSerialEventTarget(), "test",
            [](int val) { return val + 0x18; })
      ->MapErr(GetCurrentSerialEventTarget(), "test",
               [&](double val) {
                 ran_err = true;
                 return Ok{};
               })
      ->Map(GetCurrentSerialEventTarget(), "test", [&](int val) {
        value = val;
        return Ok{};
      });

  NS_ProcessPendingEvents(nullptr);

  EXPECT_EQ(value, 42);
  EXPECT_EQ(ran_err, false);
}

TEST(MozPromise, MapErr)
{
  bool ran_ok = false;
  double result = 0.0;

  InvokeAsync(GetCurrentSerialEventTarget(), "test",
              [&]() { return TestPromise::CreateAndReject(1.0, "test"); })
      ->Map(GetCurrentSerialEventTarget(), "test",
            [&](int val) {
              ran_ok = true;
              return 1;
            })
      ->MapErr(GetCurrentSerialEventTarget(), "test",
               [](double val) { return val * 2; })
      ->MapErr(GetCurrentSerialEventTarget(), "test", [&](double val) {
        result = val;
        return Ok{};
      });

  NS_ProcessPendingEvents(nullptr);

  EXPECT_EQ(result, 2.0);
  EXPECT_EQ(ran_ok, false);
}

TEST(MozPromise, ObjectDestructionOrder)
{
  AutoTaskQueue atq;
  RefPtr<TaskQueue> queue = atq.Queue();

  nsTArray<size_t> list;

  bool done = false;

  InvokeAsync(GetCurrentSerialEventTarget(), __func__,
              [object = MakeUnique<DtorTracker>(list, 0u)]() {
                return TestPromise::CreateAndResolve(42, __func__);
              })
      ->Then(queue, __func__,
             [object = MakeUnique<DtorTracker>(list, 1u)](
                 const TestPromise::ResolveOrRejectValue& aValue) {
               ChaosMode::enterChaosMode();
               return TestPromise::CreateAndResolveOrReject(aValue, __func__);
             })
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [object = MakeUnique<DtorTracker>(list, 2u)](
                 const TestPromise::ResolveOrRejectValue& aValue) {
               return TestPromise::CreateAndResolveOrReject(aValue, __func__);
             })
      ->Then(queue, __func__,
             [object = MakeUnique<DtorTracker>(list, 3u)](
                 const TestPromise::ResolveOrRejectValue& aValue) {
               ChaosMode::leaveChaosMode();
               return TestPromise::CreateAndResolveOrReject(aValue, __func__);
             })
      ->Then(GetCurrentSerialEventTarget(), __func__,
             [object = MakeUnique<DtorTracker>(list, 4u),
              &done](const TestPromise::ResolveOrRejectValue& aValue) {
               done = true;
               return TestPromise::CreateAndResolveOrReject(aValue, __func__);
             });

  MOZ_ALWAYS_TRUE(
      SpinEventLoopUntil("xpcom:TEST(MozPromise, ObjectDestructionOrder)"_ns,
                         [&done]() { return done; }));

  EXPECT_EQ(list.Length(), 5u);

  for (size_t i = 0u; i < 5u; i++) {
    EXPECT_EQ(list.SafeElementAt(i, -1), i);
  }

  queue->BeginShutdown();
}

#undef DO_FAIL
