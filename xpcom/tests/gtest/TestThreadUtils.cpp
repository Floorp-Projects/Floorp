/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http:mozilla.org/MPL/2.0/. */

#include "nsThreadUtils.h"

#include "gtest/gtest.h"

using namespace mozilla;

enum {
  TEST_CALL_VOID_ARG_VOID_RETURN,
  TEST_CALL_VOID_ARG_VOID_RETURN_CONST,
  TEST_CALL_VOID_ARG_NONVOID_RETURN,
  TEST_CALL_NONVOID_ARG_VOID_RETURN,
  TEST_CALL_NONVOID_ARG_NONVOID_RETURN,
  TEST_CALL_NONVOID_ARG_VOID_RETURN_EXPLICIT,
  TEST_CALL_NONVOID_ARG_NONVOID_RETURN_EXPLICIT,
#ifdef HAVE_STDCALL
  TEST_STDCALL_VOID_ARG_VOID_RETURN,
  TEST_STDCALL_VOID_ARG_NONVOID_RETURN,
  TEST_STDCALL_NONVOID_ARG_VOID_RETURN,
  TEST_STDCALL_NONVOID_ARG_NONVOID_RETURN,
  TEST_STDCALL_NONVOID_ARG_NONVOID_RETURN_EXPLICIT,
#endif
  TEST_CALL_NEWTHREAD_SUICIDAL,
  MAX_TESTS
};

bool gRunnableExecuted[MAX_TESTS];

class nsFoo : public nsISupports {
  NS_DECL_ISUPPORTS
  nsresult DoFoo(bool* aBool) {
    *aBool = true;
    return NS_OK;
  }

private:
  virtual ~nsFoo() {}
};

NS_IMPL_ISUPPORTS0(nsFoo)

class TestSuicide : public mozilla::Runnable {
  NS_IMETHOD Run() override {
    // Runs first time on thread "Suicide", then dies on MainThread
    if (!NS_IsMainThread()) {
      mThread = do_GetCurrentThread();
      NS_DispatchToMainThread(this);
      return NS_OK;
    }
    MOZ_RELEASE_ASSERT(mThread);
    mThread->Shutdown();
    gRunnableExecuted[TEST_CALL_NEWTHREAD_SUICIDAL] = true;
    return NS_OK;
  }

private:
  nsCOMPtr<nsIThread> mThread;
};

class nsBar : public nsISupports {
  virtual ~nsBar() {}
public:
  NS_DECL_ISUPPORTS
  void DoBar1(void) {
    gRunnableExecuted[TEST_CALL_VOID_ARG_VOID_RETURN] = true;
  }
  void DoBar1Const(void) const {
    gRunnableExecuted[TEST_CALL_VOID_ARG_VOID_RETURN_CONST] = true;
  }
  nsresult DoBar2(void) {
    gRunnableExecuted[TEST_CALL_VOID_ARG_NONVOID_RETURN] = true;
    return NS_OK;
  }
  void DoBar3(nsFoo* aFoo) {
    aFoo->DoFoo(&gRunnableExecuted[TEST_CALL_NONVOID_ARG_VOID_RETURN]);
  }
  nsresult DoBar4(nsFoo* aFoo) {
    return aFoo->DoFoo(&gRunnableExecuted[TEST_CALL_NONVOID_ARG_NONVOID_RETURN]);
  }
  void DoBar5(nsFoo* aFoo) {
    if (aFoo)
      gRunnableExecuted[TEST_CALL_NONVOID_ARG_VOID_RETURN_EXPLICIT] = true;
  }
  nsresult DoBar6(char* aFoo) {
    if (strlen(aFoo))
      gRunnableExecuted[TEST_CALL_NONVOID_ARG_NONVOID_RETURN_EXPLICIT] = true;
    return NS_OK;
  }
#ifdef HAVE_STDCALL
  void __stdcall DoBar1std(void) {
    gRunnableExecuted[TEST_STDCALL_VOID_ARG_VOID_RETURN] = true;
  }
  nsresult __stdcall DoBar2std(void) {
    gRunnableExecuted[TEST_STDCALL_VOID_ARG_NONVOID_RETURN] = true;
    return NS_OK;
  }
  void __stdcall DoBar3std(nsFoo* aFoo) {
    aFoo->DoFoo(&gRunnableExecuted[TEST_STDCALL_NONVOID_ARG_VOID_RETURN]);
  }
  nsresult __stdcall DoBar4std(nsFoo* aFoo) {
    return aFoo->DoFoo(&gRunnableExecuted[TEST_STDCALL_NONVOID_ARG_NONVOID_RETURN]);
  }
  void __stdcall DoBar5std(nsFoo* aFoo) {
    if (aFoo)
      gRunnableExecuted[TEST_STDCALL_NONVOID_ARG_VOID_RETURN_EXPLICIT] = true;
  }
  nsresult __stdcall DoBar6std(char* aFoo) {
    if (strlen(aFoo))
      gRunnableExecuted[TEST_CALL_NONVOID_ARG_VOID_RETURN_EXPLICIT] = true;
    return NS_OK;
  }
#endif
};

NS_IMPL_ISUPPORTS0(nsBar)

struct TestCopyWithNoMove
{
  explicit TestCopyWithNoMove(int* aCopyCounter) : mCopyCounter(aCopyCounter) {}
  TestCopyWithNoMove(const TestCopyWithNoMove& a) : mCopyCounter(a.mCopyCounter) { ++mCopyCounter; };
  // No 'move' declaration, allows passing object by rvalue copy.
  // Destructor nulls member variable...
  ~TestCopyWithNoMove() { mCopyCounter = nullptr; }
  // ... so we can check that the object is called when still alive.
  void operator()() { MOZ_RELEASE_ASSERT(mCopyCounter); }
  int* mCopyCounter;
};
struct TestCopyWithDeletedMove
{
  explicit TestCopyWithDeletedMove(int* aCopyCounter) : mCopyCounter(aCopyCounter) {}
  TestCopyWithDeletedMove(const TestCopyWithDeletedMove& a) : mCopyCounter(a.mCopyCounter) { ++mCopyCounter; };
  // Deleted move prevents passing by rvalue (even if copy would work)
  TestCopyWithDeletedMove(TestCopyWithDeletedMove&&) = delete;
  ~TestCopyWithDeletedMove() { mCopyCounter = nullptr; }
  void operator()() { MOZ_RELEASE_ASSERT(mCopyCounter); }
  int* mCopyCounter;
};
struct TestMove
{
  explicit TestMove(int* aMoveCounter) : mMoveCounter(aMoveCounter) {}
  TestMove(const TestMove&) = delete;
  TestMove(TestMove&& a) : mMoveCounter(a.mMoveCounter) { a.mMoveCounter = nullptr; ++mMoveCounter; }
  ~TestMove() { mMoveCounter = nullptr; }
  void operator()() { MOZ_RELEASE_ASSERT(mMoveCounter); }
  int* mMoveCounter;
};
struct TestCopyMove
{
  TestCopyMove(int* aCopyCounter, int* aMoveCounter) : mCopyCounter(aCopyCounter), mMoveCounter(aMoveCounter) {}
  TestCopyMove(const TestCopyMove& a) : mCopyCounter(a.mCopyCounter), mMoveCounter(a.mMoveCounter) { ++mCopyCounter; };
  TestCopyMove(TestCopyMove&& a) : mCopyCounter(a.mCopyCounter), mMoveCounter(a.mMoveCounter) { a.mMoveCounter = nullptr; ++mMoveCounter; }
  ~TestCopyMove() { mCopyCounter = nullptr; mMoveCounter = nullptr; }
  void operator()() { MOZ_RELEASE_ASSERT(mCopyCounter); MOZ_RELEASE_ASSERT(mMoveCounter); }
  int* mCopyCounter;
  int* mMoveCounter;
};

static void Expect(const char* aContext, int aCounter, int aMaxExpected)
{
  EXPECT_LE(aCounter, aMaxExpected) << aContext;
}

TEST(ThreadUtils, NewRunnableFunction)
{
  // Test NS_NewRunnableFunction with copyable-only function object.
  {
    int copyCounter = 0;
    {
      nsCOMPtr<nsIRunnable> trackedRunnable;
      {
        TestCopyWithNoMove tracker(&copyCounter);
        trackedRunnable = NS_NewRunnableFunction(tracker);
        // Original 'tracker' is destroyed here.
      }
      // Verify that the runnable contains a non-destroyed function object.
      trackedRunnable->Run();
    }
    Expect("NS_NewRunnableFunction with copyable-only (and no move) function, copies",
           copyCounter, 1);
  }
  {
    int copyCounter = 0;
    {
      nsCOMPtr<nsIRunnable> trackedRunnable;
      {
        // Passing as rvalue, but using copy.
        // (TestCopyWithDeletedMove wouldn't allow this.)
        trackedRunnable = NS_NewRunnableFunction(TestCopyWithNoMove(&copyCounter));
      }
      trackedRunnable->Run();
    }
    Expect("NS_NewRunnableFunction with copyable-only (and no move) function rvalue, copies",
           copyCounter, 1);
  }
  {
    int copyCounter = 0;
    {
      nsCOMPtr<nsIRunnable> trackedRunnable;
      {
        TestCopyWithDeletedMove tracker(&copyCounter);
        trackedRunnable = NS_NewRunnableFunction(tracker);
      }
      trackedRunnable->Run();
    }
    Expect("NS_NewRunnableFunction with copyable-only (and deleted move) function, copies",
           copyCounter, 1);
  }

  // Test NS_NewRunnableFunction with movable-only function object.
  {
    int moveCounter = 0;
    {
      nsCOMPtr<nsIRunnable> trackedRunnable;
      {
        TestMove tracker(&moveCounter);
        trackedRunnable = NS_NewRunnableFunction(Move(tracker));
      }
      trackedRunnable->Run();
    }
    Expect("NS_NewRunnableFunction with movable-only function, moves",
           moveCounter, 1);
  }
  {
    int moveCounter = 0;
    {
      nsCOMPtr<nsIRunnable> trackedRunnable;
      {
        trackedRunnable = NS_NewRunnableFunction(TestMove(&moveCounter));
      }
      trackedRunnable->Run();
    }
    Expect("NS_NewRunnableFunction with movable-only function rvalue, moves",
           moveCounter, 1);
  }

  // Test NS_NewRunnableFunction with copyable&movable function object.
  {
    int copyCounter = 0;
    int moveCounter = 0;
    {
      nsCOMPtr<nsIRunnable> trackedRunnable;
      {
        TestCopyMove tracker(&copyCounter, &moveCounter);
        trackedRunnable = NS_NewRunnableFunction(Move(tracker));
      }
      trackedRunnable->Run();
    }
    Expect("NS_NewRunnableFunction with copyable&movable function, copies",
           copyCounter, 0);
    Expect("NS_NewRunnableFunction with copyable&movable function, moves",
           moveCounter, 1);
  }
  {
    int copyCounter = 0;
    int moveCounter = 0;
    {
      nsCOMPtr<nsIRunnable> trackedRunnable;
      {
        trackedRunnable =
          NS_NewRunnableFunction(TestCopyMove(&copyCounter, &moveCounter));
      }
      trackedRunnable->Run();
    }
    Expect("NS_NewRunnableFunction with copyable&movable function rvalue, copies",
           copyCounter, 0);
    Expect("NS_NewRunnableFunction with copyable&movable function rvalue, moves",
           moveCounter, 1);
  }

  // Test NS_NewRunnableFunction with copyable-only lambda capture.
  {
    int copyCounter = 0;
    {
      nsCOMPtr<nsIRunnable> trackedRunnable;
      {
        TestCopyWithNoMove tracker(&copyCounter);
        // Expect 2 copies (here -> local lambda -> runnable lambda).
        trackedRunnable = NS_NewRunnableFunction([tracker]() mutable { tracker(); });
      }
      trackedRunnable->Run();
    }
    Expect("NS_NewRunnableFunction with copyable-only (and no move) capture, copies",
           copyCounter, 2);
  }
  {
    int copyCounter = 0;
    {
      nsCOMPtr<nsIRunnable> trackedRunnable;
      {
        TestCopyWithDeletedMove tracker(&copyCounter);
        // Expect 2 copies (here -> local lambda -> runnable lambda).
        trackedRunnable = NS_NewRunnableFunction([tracker]() mutable { tracker(); });
      }
      trackedRunnable->Run();
    }
    Expect("NS_NewRunnableFunction with copyable-only (and deleted move) capture, copies",
           copyCounter, 2);
  }

  // Note: Not possible to use move-only captures.
  // (Until we can use C++14 generalized lambda captures)

  // Test NS_NewRunnableFunction with copyable&movable lambda capture.
  {
    int copyCounter = 0;
    int moveCounter = 0;
    {
      nsCOMPtr<nsIRunnable> trackedRunnable;
      {
        TestCopyMove tracker(&copyCounter, &moveCounter);
        trackedRunnable = NS_NewRunnableFunction([tracker]() mutable { tracker(); });
        // Expect 1 copy (here -> local lambda) and 1 move (local -> runnable lambda).
      }
      trackedRunnable->Run();
    }
    Expect("NS_NewRunnableFunction with copyable&movable capture, copies",
           copyCounter, 1);
    Expect("NS_NewRunnableFunction with copyable&movable capture, moves",
           moveCounter, 1);
  }
}

TEST(ThreadUtils, RunnableMethod)
{
  memset(gRunnableExecuted, false, MAX_TESTS * sizeof(bool));
  // Scope the smart ptrs so that the runnables need to hold on to whatever they need
  {
    RefPtr<nsFoo> foo = new nsFoo();
    RefPtr<nsBar> bar = new nsBar();
    RefPtr<const nsBar> constBar = bar;

    // This pointer will be freed at the end of the block
    // Do not dereference this pointer in the runnable method!
    RefPtr<nsFoo> rawFoo = new nsFoo();

    // Read only string. Dereferencing in runnable method to check this works.
    char* message = (char*)"Test message";

    NS_DispatchToMainThread(NewRunnableMethod(bar, &nsBar::DoBar1));
    NS_DispatchToMainThread(NewRunnableMethod(constBar, &nsBar::DoBar1Const));
    NS_DispatchToMainThread(NewRunnableMethod(bar, &nsBar::DoBar2));
    NS_DispatchToMainThread(NewRunnableMethod<RefPtr<nsFoo>>
      (bar, &nsBar::DoBar3, foo));
    NS_DispatchToMainThread(NewRunnableMethod<RefPtr<nsFoo>>
      (bar, &nsBar::DoBar4, foo));
    NS_DispatchToMainThread(NewRunnableMethod<nsFoo*>(bar, &nsBar::DoBar5, rawFoo));
    NS_DispatchToMainThread(NewRunnableMethod<char*>(bar, &nsBar::DoBar6, message));
#ifdef HAVE_STDCALL
    NS_DispatchToMainThread(NewRunnableMethod(bar, &nsBar::DoBar1std));
    NS_DispatchToMainThread(NewRunnableMethod(bar, &nsBar::DoBar2std));
    NS_DispatchToMainThread(NewRunnableMethod<RefPtr<nsFoo>>
      (bar, &nsBar::DoBar3std, foo));
    NS_DispatchToMainThread(NewRunnableMethod<RefPtr<nsFoo>>
      (bar, &nsBar::DoBar4std, foo));
    NS_DispatchToMainThread(NewRunnableMethod<nsFoo*>(bar, &nsBar::DoBar5std, rawFoo));
    NS_DispatchToMainThread(NewRunnableMethod<char*>(bar, &nsBar::DoBar6std, message));
#endif
  }

  // Spin the event loop
  NS_ProcessPendingEvents(nullptr);

  // Now test a suicidal event in NS_New(Named)Thread
  nsCOMPtr<nsIThread> thread;
  NS_NewNamedThread("SuicideThread", getter_AddRefs(thread), new TestSuicide());
  ASSERT_TRUE(thread);

  while (!gRunnableExecuted[TEST_CALL_NEWTHREAD_SUICIDAL]) {
    NS_ProcessPendingEvents(nullptr);
  }

  for (uint32_t i = 0; i < MAX_TESTS; i++) {
    EXPECT_TRUE(gRunnableExecuted[i]) << "Error in test " << i;
  }
}
