/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http:mozilla.org/MPL/2.0/. */

#include "nsThreadUtils.h"
#include "mozilla/UniquePtr.h"

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

static void ExpectRunnableName(Runnable* aRunnable, const char* aExpectedName)
{
  nsAutoCString name;
  EXPECT_TRUE(NS_SUCCEEDED(aRunnable->GetName(name))) << "Runnable::GetName()";
#ifdef RELEASE_OR_BETA
  EXPECT_TRUE(name.IsEmpty()) << "Runnable name shall be empty in RELEASE or BETA!";
#else
  EXPECT_TRUE(name.EqualsASCII(aExpectedName)) << "Verify Runnable name";
#endif
}

static void TestNewRunnableFunction(bool aNamed)
{
  // Test NS_NewRunnableFunction with copyable-only function object.
  {
    int copyCounter = 0;
    {
      nsCOMPtr<nsIRunnable> trackedRunnable;
      {
        TestCopyWithNoMove tracker(&copyCounter);
        trackedRunnable = aNamed ? NS_NewRunnableFunction("unused", tracker) :
                                   NS_NewRunnableFunction(tracker);
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
        trackedRunnable = aNamed ?
          NS_NewRunnableFunction("unused", TestCopyWithNoMove(&copyCounter)) :
          NS_NewRunnableFunction(TestCopyWithNoMove(&copyCounter));
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
        trackedRunnable = aNamed ? NS_NewRunnableFunction("unused", tracker) :
                                   NS_NewRunnableFunction(tracker);
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
        trackedRunnable = aNamed ? NS_NewRunnableFunction("unused", Move(tracker)) :
                                   NS_NewRunnableFunction(Move(tracker));
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
        trackedRunnable = aNamed ?
          NS_NewRunnableFunction("unused", TestMove(&moveCounter)) :
          NS_NewRunnableFunction(TestMove(&moveCounter));
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
        trackedRunnable = aNamed ? NS_NewRunnableFunction("unused", Move(tracker)) :
                                   NS_NewRunnableFunction(Move(tracker));
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
        trackedRunnable = aNamed ?
          NS_NewRunnableFunction("unused", TestCopyMove(&copyCounter, &moveCounter)) :
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
        trackedRunnable = aNamed ?
          NS_NewRunnableFunction("unused", [tracker]() mutable { tracker(); }) :
          NS_NewRunnableFunction([tracker]() mutable { tracker(); });
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
        trackedRunnable = aNamed ?
          NS_NewRunnableFunction("unused", [tracker]() mutable { tracker(); }) :
          NS_NewRunnableFunction([tracker]() mutable { tracker(); });
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
        trackedRunnable = aNamed ?
          NS_NewRunnableFunction("unused", [tracker]() mutable { tracker(); }) :
          NS_NewRunnableFunction([tracker]() mutable { tracker(); });
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

TEST(ThreadUtils, NewRunnableFunction)
{
  TestNewRunnableFunction(/*aNamed*/false);
}

TEST(ThreadUtils, NewNamedRunnableFunction)
{
  // The named overload shall behave identical to the non-named counterpart.
  TestNewRunnableFunction(/*aNamed*/true);

  // Test naming.
  {
    const char* expectedName = "NamedRunnable";
    RefPtr<Runnable> NamedRunnable = NS_NewRunnableFunction(expectedName, []{});
    ExpectRunnableName(NamedRunnable, expectedName);
  }
}

static void TestNewRunnableMethod(bool aNamed)
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

    NS_DispatchToMainThread(aNamed ?
      NewRunnableMethod("unused", bar, &nsBar::DoBar1) :
      NewRunnableMethod(bar, &nsBar::DoBar1));
    NS_DispatchToMainThread(aNamed ?
      NewRunnableMethod("unused", constBar, &nsBar::DoBar1Const) :
      NewRunnableMethod(constBar, &nsBar::DoBar1Const));
    NS_DispatchToMainThread(aNamed ?
      NewRunnableMethod("unused", bar, &nsBar::DoBar2) :
      NewRunnableMethod(bar, &nsBar::DoBar2));
    NS_DispatchToMainThread(aNamed ?
      NewRunnableMethod<RefPtr<nsFoo>>("unused", bar, &nsBar::DoBar3, foo) :
      NewRunnableMethod<RefPtr<nsFoo>>(bar, &nsBar::DoBar3, foo));
    NS_DispatchToMainThread(aNamed ?
      NewRunnableMethod<RefPtr<nsFoo>>("unused", bar, &nsBar::DoBar4, foo) :
      NewRunnableMethod<RefPtr<nsFoo>>(bar, &nsBar::DoBar4, foo));
    NS_DispatchToMainThread(aNamed ?
      NewRunnableMethod<nsFoo*>("unused", bar, &nsBar::DoBar5, rawFoo) :
      NewRunnableMethod<nsFoo*>(bar, &nsBar::DoBar5, rawFoo));
    NS_DispatchToMainThread(aNamed ?
      NewRunnableMethod<char*>("unused", bar, &nsBar::DoBar6, message) :
      NewRunnableMethod<char*>(bar, &nsBar::DoBar6, message));
#ifdef HAVE_STDCALL
    NS_DispatchToMainThread(aNamed ?
      NewRunnableMethod("unused", bar, &nsBar::DoBar1std) :
      NewRunnableMethod(bar, &nsBar::DoBar1std));
    NS_DispatchToMainThread(aNamed ?
      NewRunnableMethod("unused", bar, &nsBar::DoBar2std) :
      NewRunnableMethod(bar, &nsBar::DoBar2std));
    NS_DispatchToMainThread(aNamed ?
      NewRunnableMethod<RefPtr<nsFoo>>("unused", bar, &nsBar::DoBar3std, foo) :
      NewRunnableMethod<RefPtr<nsFoo>>(bar, &nsBar::DoBar3std, foo));
    NS_DispatchToMainThread(aNamed ?
      NewRunnableMethod<RefPtr<nsFoo>>("unused", bar, &nsBar::DoBar4std, foo) :
      NewRunnableMethod<RefPtr<nsFoo>>(bar, &nsBar::DoBar4std, foo));
    NS_DispatchToMainThread(aNamed ?
      NewRunnableMethod<nsFoo*>("unused", bar, &nsBar::DoBar5std, rawFoo) :
      NewRunnableMethod<nsFoo*>(bar, &nsBar::DoBar5std, rawFoo));
    NS_DispatchToMainThread(aNamed ?
      NewRunnableMethod<char*>("unused", bar, &nsBar::DoBar6std, message) :
      NewRunnableMethod<char*>(bar, &nsBar::DoBar6std, message));
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

TEST(ThreadUtils, RunnableMethod)
{
  TestNewRunnableMethod(/* aNamed */false);
}

TEST(ThreadUtils, NamedRunnableMethod)
{
  // The named overloads shall behave identical to the non-named counterparts.
  TestNewRunnableMethod(/* aNamed */true);

  // Test naming.
  {
    RefPtr<nsFoo> foo = new nsFoo();
    const char* expectedName = "NamedRunnable";
    bool unused;
    RefPtr<Runnable> NamedRunnable =
      NewRunnableMethod<bool*>(expectedName, foo, &nsFoo::DoFoo, &unused);
    ExpectRunnableName(NamedRunnable, expectedName);
  }
}

class IdleObjectWithoutSetDeadline final
{
public:
  NS_INLINE_DECL_REFCOUNTING(IdleObjectWithoutSetDeadline)
  IdleObjectWithoutSetDeadline()
    : mRunnableExecuted(false)
  {
  }
  void Method() { mRunnableExecuted = true; }
  bool mRunnableExecuted;

private:
  ~IdleObjectWithoutSetDeadline() {}
};

class IdleObjectParentWithSetDeadline
{
public:
  IdleObjectParentWithSetDeadline() : mSetDeadlineCalled(false) {}
  void SetDeadline(TimeStamp aDeadline) { mSetDeadlineCalled = true; }
  bool mSetDeadlineCalled;
};

class IdleObjectInheritedSetDeadline final
  : public IdleObjectParentWithSetDeadline
{
public:
  NS_INLINE_DECL_REFCOUNTING(IdleObjectInheritedSetDeadline)
  IdleObjectInheritedSetDeadline()
    : mRunnableExecuted(false)
  {
  }
  void Method() { mRunnableExecuted = true; }
  bool mRunnableExecuted;

private:
  ~IdleObjectInheritedSetDeadline() {}
};

class IdleObject final
{
public:
  NS_INLINE_DECL_REFCOUNTING(IdleObject)
  IdleObject()
  {
    for (uint32_t index = 0; index < ArrayLength(mRunnableExecuted); ++index) {
      mRunnableExecuted[index] = false;
      mSetIdleDeadlineCalled = false;
    }
  }
  void SetDeadline(TimeStamp aTimeStamp) {
    mSetIdleDeadlineCalled = true;
  }

  void CheckExecutedMethods(const char* aKey, uint32_t aNumExecuted)
  {
    uint32_t index;
    for (index = 0; index < aNumExecuted; ++index) {
      ASSERT_TRUE(mRunnableExecuted[index])
          << aKey << ": Method" << index << " should've executed";
    }

    for (; index < ArrayLength(mRunnableExecuted); ++index) {
      ASSERT_FALSE(mRunnableExecuted[index])
        << aKey << ": Method" << index << " shouldn't have executed";
    }
  }

  void Method0()
  {
    CheckExecutedMethods("Method0", 0);
    mRunnableExecuted[0] = true;
    mSetIdleDeadlineCalled = false;
  }

  void Method1()
  {
    CheckExecutedMethods("Method1", 1);
    ASSERT_TRUE(mSetIdleDeadlineCalled);
    mRunnableExecuted[1] = true;
    mSetIdleDeadlineCalled = false;
  }

  void Method2()
  {
    CheckExecutedMethods("Method2", 2);
    ASSERT_TRUE(mSetIdleDeadlineCalled);
    mRunnableExecuted[2] = true;
    mSetIdleDeadlineCalled = false;
    NS_DispatchToCurrentThread(NewRunnableMethod(this, &IdleObject::Method3));
  }

  void Method3()
  {
    CheckExecutedMethods("Method3", 3);

    mTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
    mTimer->InitWithFuncCallback(Method4, this, 10, nsITimer::TYPE_ONE_SHOT);
    NS_IdleDispatchToCurrentThread(
      NewIdleRunnableMethodWithTimer(this, &IdleObject::Method5), 50);
    NS_IdleDispatchToCurrentThread(
      NewRunnableMethod(this, &IdleObject::Method6), 100);

    PR_Sleep(PR_MillisecondsToInterval(200));
    mRunnableExecuted[3] = true;
    mSetIdleDeadlineCalled = false;
  }

  static void Method4(nsITimer* aTimer, void* aClosure)
  {
    RefPtr<IdleObject> self = static_cast<IdleObject*>(aClosure);
    self->CheckExecutedMethods("Method4", 4);
    self->mRunnableExecuted[4] = true;
    self->mSetIdleDeadlineCalled = false;
  }

  void Method5()
  {
    CheckExecutedMethods("Method5", 5);
    ASSERT_TRUE(mSetIdleDeadlineCalled);
    mRunnableExecuted[5] = true;
    mSetIdleDeadlineCalled = false;
  }

  void Method6()
  {
    CheckExecutedMethods("Method6", 6);
    mRunnableExecuted[6] = true;
    mSetIdleDeadlineCalled = false;
  }

  void Method7()
  {
    CheckExecutedMethods("Method7", 7);
    ASSERT_TRUE(mSetIdleDeadlineCalled);
    mRunnableExecuted[7] = true;
    mSetIdleDeadlineCalled = false;
  }

private:
  nsCOMPtr<nsITimer> mTimer;
  bool mRunnableExecuted[8];
  bool mSetIdleDeadlineCalled;
  ~IdleObject() {}
};

TEST(ThreadUtils, IdleRunnableMethod)
{
  {
    RefPtr<IdleObject> idle = new IdleObject();
    RefPtr<IdleObjectWithoutSetDeadline> idleNoSetDeadline =
      new IdleObjectWithoutSetDeadline();
    RefPtr<IdleObjectInheritedSetDeadline> idleInheritedSetDeadline =
      new IdleObjectInheritedSetDeadline();

    NS_DispatchToCurrentThread(NewRunnableMethod(idle, &IdleObject::Method0));
    NS_IdleDispatchToCurrentThread(
      NewIdleRunnableMethod(idle, &IdleObject::Method1));
    NS_IdleDispatchToCurrentThread(
      NewIdleRunnableMethodWithTimer(idle, &IdleObject::Method2), 60000);
    NS_IdleDispatchToCurrentThread(
      NewIdleRunnableMethod(idle, &IdleObject::Method7));
    NS_IdleDispatchToCurrentThread(NewIdleRunnableMethod<const char*, uint32_t>(
      idle, &IdleObject::CheckExecutedMethods, "final", 8));
    NS_IdleDispatchToCurrentThread(NewIdleRunnableMethod(
      idleNoSetDeadline, &IdleObjectWithoutSetDeadline::Method));
    NS_IdleDispatchToCurrentThread(NewIdleRunnableMethod(
      idleInheritedSetDeadline, &IdleObjectInheritedSetDeadline::Method));

    NS_ProcessPendingEvents(nullptr);

    ASSERT_TRUE(idleNoSetDeadline->mRunnableExecuted);
    ASSERT_TRUE(idleInheritedSetDeadline->mRunnableExecuted);
    ASSERT_TRUE(idleInheritedSetDeadline->mSetDeadlineCalled);
  }
}

// {9e70a320-be02-11d1-8031-006008159b5a}
#define NS_IFOO_IID \
  {0x9e70a320, 0xbe02, 0x11d1,    \
    {0x80, 0x31, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a}}

TEST(ThreadUtils, TypeTraits)
{
  static_assert(!mozilla::IsRefcountedSmartPointer<int>::value,
                "IsRefcountedSmartPointer<int> should be false");
  static_assert(mozilla::IsRefcountedSmartPointer<RefPtr<int>>::value,
                "IsRefcountedSmartPointer<RefPtr<...>> should be true");
  static_assert(mozilla::IsRefcountedSmartPointer<const RefPtr<int>>::value,
                "IsRefcountedSmartPointer<const RefPtr<...>> should be true");
  static_assert(mozilla::IsRefcountedSmartPointer<volatile RefPtr<int>>::value,
                "IsRefcountedSmartPointer<volatile RefPtr<...>> should be true");
  static_assert(mozilla::IsRefcountedSmartPointer<const volatile RefPtr<int>>::value,
               "IsRefcountedSmartPointer<const volatile RefPtr<...>> should be true");
  static_assert(mozilla::IsRefcountedSmartPointer<nsCOMPtr<int>>::value,
                "IsRefcountedSmartPointer<nsCOMPtr<...>> should be true");
  static_assert(mozilla::IsRefcountedSmartPointer<const nsCOMPtr<int>>::value,
                "IsRefcountedSmartPointer<const nsCOMPtr<...>> should be true");
  static_assert(mozilla::IsRefcountedSmartPointer<volatile nsCOMPtr<int>>::value,
                "IsRefcountedSmartPointer<volatile nsCOMPtr<...>> should be true");
  static_assert(mozilla::IsRefcountedSmartPointer<const volatile nsCOMPtr<int>>::value,
                "IsRefcountedSmartPointer<const volatile nsCOMPtr<...>> should be true");

  static_assert(mozilla::IsSame<int, mozilla::RemoveSmartPointer<int>::Type>::value,
                "RemoveSmartPointer<int>::Type should be int");
  static_assert(mozilla::IsSame<int*, mozilla::RemoveSmartPointer<int*>::Type>::value,
                "RemoveSmartPointer<int*>::Type should be int*");
  static_assert(mozilla::IsSame<UniquePtr<int>, mozilla::RemoveSmartPointer<UniquePtr<int>>::Type>::value,
                "RemoveSmartPointer<UniquePtr<int>>::Type should be UniquePtr<int>");
  static_assert(mozilla::IsSame<int, mozilla::RemoveSmartPointer<RefPtr<int>>::Type>::value,
                "RemoveSmartPointer<RefPtr<int>>::Type should be int");
  static_assert(mozilla::IsSame<int, mozilla::RemoveSmartPointer<const RefPtr<int>>::Type>::value,
                "RemoveSmartPointer<const RefPtr<int>>::Type should be int");
  static_assert(mozilla::IsSame<int, mozilla::RemoveSmartPointer<volatile RefPtr<int>>::Type>::value,
                "RemoveSmartPointer<volatile RefPtr<int>>::Type should be int");
  static_assert(mozilla::IsSame<int, mozilla::RemoveSmartPointer<const volatile RefPtr<int>>::Type>::value,
                "RemoveSmartPointer<const volatile RefPtr<int>>::Type should be int");
  static_assert(mozilla::IsSame<int, mozilla::RemoveSmartPointer<nsCOMPtr<int>>::Type>::value,
                "RemoveSmartPointer<nsCOMPtr<int>>::Type should be int");
  static_assert(mozilla::IsSame<int, mozilla::RemoveSmartPointer<const nsCOMPtr<int>>::Type>::value,
                "RemoveSmartPointer<const nsCOMPtr<int>>::Type should be int");
  static_assert(mozilla::IsSame<int, mozilla::RemoveSmartPointer<volatile nsCOMPtr<int>>::Type>::value,
                "RemoveSmartPointer<volatile nsCOMPtr<int>>::Type should be int");
  static_assert(mozilla::IsSame<int, mozilla::RemoveSmartPointer<const volatile nsCOMPtr<int>>::Type>::value,
                "RemoveSmartPointer<const volatile nsCOMPtr<int>>::Type should be int");

  static_assert(mozilla::IsSame<int, mozilla::RemoveRawOrSmartPointer<int>::Type>::value,
                "RemoveRawOrSmartPointer<int>::Type should be int");
  static_assert(mozilla::IsSame<UniquePtr<int>, mozilla::RemoveRawOrSmartPointer<UniquePtr<int>>::Type>::value,
                "RemoveRawOrSmartPointer<UniquePtr<int>>::Type should be UniquePtr<int>");
  static_assert(mozilla::IsSame<int, mozilla::RemoveRawOrSmartPointer<int*>::Type>::value,
                "RemoveRawOrSmartPointer<int*>::Type should be int");
  static_assert(mozilla::IsSame<const int, mozilla::RemoveRawOrSmartPointer<const int*>::Type>::value,
                "RemoveRawOrSmartPointer<const int*>::Type should be const int");
  static_assert(mozilla::IsSame<volatile int, mozilla::RemoveRawOrSmartPointer<volatile int*>::Type>::value,
                "RemoveRawOrSmartPointer<volatile int*>::Type should be volatile int");
  static_assert(mozilla::IsSame<const volatile int, mozilla::RemoveRawOrSmartPointer<const volatile int*>::Type>::value,
                "RemoveRawOrSmartPointer<const volatile int*>::Type should be const volatile int");
  static_assert(mozilla::IsSame<int, mozilla::RemoveRawOrSmartPointer<RefPtr<int>>::Type>::value,
                "RemoveRawOrSmartPointer<RefPtr<int>>::Type should be int");
  static_assert(mozilla::IsSame<int, mozilla::RemoveRawOrSmartPointer<const RefPtr<int>>::Type>::value,
                "RemoveRawOrSmartPointer<const RefPtr<int>>::Type should be int");
  static_assert(mozilla::IsSame<int, mozilla::RemoveRawOrSmartPointer<volatile RefPtr<int>>::Type>::value,
                "RemoveRawOrSmartPointer<volatile RefPtr<int>>::Type should be int");
  static_assert(mozilla::IsSame<int, mozilla::RemoveRawOrSmartPointer<const volatile RefPtr<int>>::Type>::value,
                "RemoveRawOrSmartPointer<const volatile RefPtr<int>>::Type should be int");
  static_assert(mozilla::IsSame<int, mozilla::RemoveRawOrSmartPointer<nsCOMPtr<int>>::Type>::value,
                "RemoveRawOrSmartPointer<nsCOMPtr<int>>::Type should be int");
  static_assert(mozilla::IsSame<int, mozilla::RemoveRawOrSmartPointer<const nsCOMPtr<int>>::Type>::value,
                "RemoveRawOrSmartPointer<const nsCOMPtr<int>>::Type should be int");
  static_assert(mozilla::IsSame<int, mozilla::RemoveRawOrSmartPointer<volatile nsCOMPtr<int>>::Type>::value,
                "RemoveRawOrSmartPointer<volatile nsCOMPtr<int>>::Type should be int");
  static_assert(mozilla::IsSame<int, mozilla::RemoveRawOrSmartPointer<const volatile nsCOMPtr<int>>::Type>::value,
                "RemoveRawOrSmartPointer<const volatile nsCOMPtr<int>>::Type should be int");
}

namespace TestThreadUtils {

static bool gDebug = false;
static int gAlive, gZombies;
static int gAllConstructions, gConstructions, gCopyConstructions,
           gMoveConstructions, gDestructions, gAssignments, gMoves;
struct Spy
{
  static void ClearActions()
  {
    gAllConstructions = gConstructions = gCopyConstructions
        = gMoveConstructions = gDestructions = gAssignments = gMoves = 0;
  }
  static void ClearAll()
  {
    ClearActions();
    gAlive = 0;
  }

  explicit Spy(int aID) : mID(aID)
  {
    ++gAlive; ++gAllConstructions; ++gConstructions;
    if (gDebug) { printf("Spy[%d@%p]()\n", mID, this); }
  }
  Spy(const Spy& o) : mID(o.mID)
  {
    ++gAlive; ++gAllConstructions; ++gCopyConstructions;
    if (gDebug) { printf("Spy[%d@%p](&[%d@%p])\n", mID, this, o.mID, &o); }
  }
  Spy(Spy&& o) : mID(o.mID)
  {
    o.mID = -o.mID;
    ++gZombies; ++gAllConstructions; ++gMoveConstructions;
    if (gDebug) { printf("Spy[%d@%p](&&[%d->%d@%p])\n", mID, this, -o.mID, o.mID, &o); }
  }
  ~Spy()
  {
    if (mID >= 0) { --gAlive; } else { --gZombies; } ++gDestructions;
    if (gDebug) { printf("~Spy[%d@%p]()\n", mID, this); }
    mID = 0;
  }
  Spy& operator=(const Spy& o)
  {
    ++gAssignments;
    if (gDebug) { printf("Spy[%d->%d@%p] = &[%d@%p]\n", mID, o.mID, this, o.mID, &o); }
    mID = o.mID;
    return *this;
  };
  Spy& operator=(Spy&& o)
  {
    --gAlive; ++gZombies;
    ++gMoves;
    if (gDebug) { printf("Spy[%d->%d@%p] = &&[%d->%d@%p]\n", mID, o.mID, this, o.mID, -o.mID, &o); }
    mID = o.mID; o.mID = -o.mID;
    return *this;
  };

  int mID; // ID given at construction, or negation if was moved from; 0 when destroyed.
};

struct ISpyWithISupports : public nsISupports
{
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IFOO_IID)
  NS_IMETHOD_(nsrefcnt) RefCnt() = 0;
  NS_IMETHOD_(int32_t) ID() = 0;
};
NS_DEFINE_STATIC_IID_ACCESSOR(ISpyWithISupports, NS_IFOO_IID)
struct SpyWithISupports : public ISpyWithISupports, public Spy
{
private:
  virtual ~SpyWithISupports() = default;
public:
  explicit SpyWithISupports(int aID) : Spy(aID) {};
  NS_DECL_ISUPPORTS
  NS_IMETHOD_(nsrefcnt) RefCnt() override { return mRefCnt; }
  NS_IMETHOD_(int32_t) ID() override { return mID; }
};
NS_IMPL_ISUPPORTS(SpyWithISupports, ISpyWithISupports)


class IThreadUtilsObject : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IFOO_IID)

  NS_IMETHOD_(nsrefcnt) RefCnt() = 0;
  NS_IMETHOD_(int32_t) ID() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(IThreadUtilsObject, NS_IFOO_IID)

struct ThreadUtilsObjectNonRefCountedBase
{
  virtual void MethodFromNonRefCountedBase() {}
};

struct ThreadUtilsObject : public IThreadUtilsObject
                         , public ThreadUtilsObjectNonRefCountedBase
{
  // nsISupports implementation
  NS_DECL_ISUPPORTS

  // IThreadUtilsObject implementation
  NS_IMETHOD_(nsrefcnt) RefCnt() override { return mRefCnt; }
  NS_IMETHOD_(int32_t) ID() override { return 0; }

  int mCount; // Number of calls + arguments processed.
  int mA0, mA1, mA2, mA3;
  Spy mSpy; const Spy* mSpyPtr;
  ThreadUtilsObject()
    : mCount(0)
    , mA0(0), mA1(0), mA2(0), mA3(0)
    , mSpy(1), mSpyPtr(nullptr)
  {}
private:
  virtual ~ThreadUtilsObject() = default;
public:
  void Test0() { mCount += 1; }
  void Test1i(int a0) { mCount += 2; mA0 = a0; }
  void Test2i(int a0, int a1) { mCount += 3; mA0 = a0; mA1 = a1; }
  void Test3i(int a0, int a1, int a2)
  {
    mCount += 4; mA0 = a0; mA1 = a1; mA2 = a2;
  }
  void Test4i(int a0, int a1, int a2, int a3)
  {
    mCount += 5; mA0 = a0; mA1 = a1; mA2 = a2; mA3 = a3;
  }
  void Test1pi(int* ap)
  {
    mCount += 2; mA0 = ap ? *ap : -1;
  }
  void Test1pci(const int* ap)
  {
    mCount += 2; mA0 = ap ? *ap : -1;
  }
  void Test1ri(int& ar)
  {
    mCount += 2; mA0 = ar;
  }
  void Test1rri(int&& arr)
  {
    mCount += 2; mA0 = arr;
  }
  void Test1upi(mozilla::UniquePtr<int> aup)
  {
    mCount += 2; mA0 = aup ? *aup : -1;
  }
  void Test1rupi(mozilla::UniquePtr<int>& aup)
  {
    mCount += 2; mA0 = aup ? *aup : -1;
  }
  void Test1rrupi(mozilla::UniquePtr<int>&& aup)
  {
    mCount += 2; mA0 = aup ? *aup : -1;
  }

  void Test1s(Spy) { mCount += 2; }
  void Test1ps(Spy*) { mCount += 2; }
  void Test1rs(Spy&) { mCount += 2; }
  void Test1rrs(Spy&&) { mCount += 2; }
  void Test1ups(mozilla::UniquePtr<Spy>) { mCount += 2; }
  void Test1rups(mozilla::UniquePtr<Spy>&) { mCount += 2; }
  void Test1rrups(mozilla::UniquePtr<Spy>&&) { mCount += 2; }

  // Possible parameter passing styles:
  void TestByValue(Spy s)
  {
    if (gDebug) { printf("TestByValue(Spy[%d@%p])\n", s.mID, &s); }
    mSpy = s;
  };
  void TestByConstLRef(const Spy& s)
  {
    if (gDebug) { printf("TestByConstLRef(Spy[%d@%p]&)\n", s.mID, &s); }
    mSpy = s;
  };
  void TestByRRef(Spy&& s)
  {
    if (gDebug) { printf("TestByRRef(Spy[%d@%p]&&)\n", s.mID, &s); }
    mSpy = mozilla::Move(s);
  };
  void TestByLRef(Spy& s)
  {
    if (gDebug) { printf("TestByLRef(Spy[%d@%p]&)\n", s.mID, &s); }
    mSpy = s;
    mSpyPtr = &s;
  };
  void TestByPointer(Spy* p)
  {
    if (p) {
      if (gDebug) { printf("TestByPointer(&Spy[%d@%p])\n", p->mID, p); }
      mSpy = *p;
    } else {
      if (gDebug) { printf("TestByPointer(nullptr)\n"); }
    }
    mSpyPtr = p;
  };
  void TestByPointerToConst(const Spy* p)
  {
    if (p) {
      if (gDebug) { printf("TestByPointerToConst(&Spy[%d@%p])\n", p->mID, p); }
      mSpy = *p;
    } else {
      if (gDebug) { printf("TestByPointerToConst(nullptr)\n"); }
    }
    mSpyPtr = p;
  };
};

NS_IMPL_ISUPPORTS(ThreadUtilsObject, IThreadUtilsObject)

class ThreadUtilsRefCountedFinal final
{
public:
  ThreadUtilsRefCountedFinal() : m_refCount(0) {}
  ~ThreadUtilsRefCountedFinal() {}
  // 'AddRef' and 'Release' methods with different return types, to verify
  // that the return type doesn't influence storage selection.
  long AddRef(void) { return ++m_refCount; }
  void Release(void) { --m_refCount; }
private:
  long m_refCount;
};

class ThreadUtilsRefCountedBase
{
public:
  ThreadUtilsRefCountedBase() : m_refCount(0) {}
  virtual ~ThreadUtilsRefCountedBase() {}
  // 'AddRef' and 'Release' methods with different return types, to verify
  // that the return type doesn't influence storage selection.
  virtual void AddRef(void) { ++m_refCount; }
  virtual MozExternalRefCountType Release(void) { return --m_refCount; }
private:
  MozExternalRefCountType m_refCount;
};

class ThreadUtilsRefCountedDerived
  : public ThreadUtilsRefCountedBase
{};

class ThreadUtilsNonRefCounted
{};

} // namespace TestThreadUtils

TEST(ThreadUtils, main)
{
  using namespace TestThreadUtils;

  static_assert(!IsParameterStorageClass<int>::value,
                "'int' should not be recognized as Storage Class");
  static_assert(IsParameterStorageClass<StoreCopyPassByValue<int>>::value,
                "StoreCopyPassByValue<int> should be recognized as Storage Class");
  static_assert(IsParameterStorageClass<StoreCopyPassByConstLRef<int>>::value,
                "StoreCopyPassByConstLRef<int> should be recognized as Storage Class");
  static_assert(IsParameterStorageClass<StoreCopyPassByLRef<int>>::value,
                "StoreCopyPassByLRef<int> should be recognized as Storage Class");
  static_assert(IsParameterStorageClass<StoreCopyPassByRRef<int>>::value,
                "StoreCopyPassByRRef<int> should be recognized as Storage Class");
  static_assert(IsParameterStorageClass<StoreRefPassByLRef<int>>::value,
                "StoreRefPassByLRef<int> should be recognized as Storage Class");
  static_assert(IsParameterStorageClass<StoreConstRefPassByConstLRef<int>>::value,
                "StoreConstRefPassByConstLRef<int> should be recognized as Storage Class");
  static_assert(IsParameterStorageClass<StoreRefPtrPassByPtr<int>>::value,
                "StoreRefPtrPassByPtr<int> should be recognized as Storage Class");
  static_assert(IsParameterStorageClass<StorePtrPassByPtr<int>>::value,
                "StorePtrPassByPtr<int> should be recognized as Storage Class");
  static_assert(IsParameterStorageClass<StoreConstPtrPassByConstPtr<int>>::value,
                "StoreConstPtrPassByConstPtr<int> should be recognized as Storage Class");
  static_assert(IsParameterStorageClass<StoreCopyPassByConstPtr<int>>::value,
                "StoreCopyPassByConstPtr<int> should be recognized as Storage Class");
  static_assert(IsParameterStorageClass<StoreCopyPassByPtr<int>>::value,
                "StoreCopyPassByPtr<int> should be recognized as Storage Class");

  RefPtr<ThreadUtilsObject> rpt(new ThreadUtilsObject);
  int count = 0;

  // Test legacy functions.

  nsCOMPtr<nsIRunnable> r1 =
    NewRunnableMethod(rpt, &ThreadUtilsObject::Test0);
  r1->Run();
  EXPECT_EQ(count += 1, rpt->mCount);

  r1 = NewRunnableMethod<int>(rpt, &ThreadUtilsObject::Test1i, 11);
  r1->Run();
  EXPECT_EQ(count += 2, rpt->mCount);
  EXPECT_EQ(11, rpt->mA0);

  // Test calling a method from a non-ref-counted base.

  r1 = NewRunnableMethod(rpt, &ThreadUtilsObject::MethodFromNonRefCountedBase);
  r1->Run();
  EXPECT_EQ(count, rpt->mCount);

  // Test variadic function with simple POD arguments.

  r1 = NewRunnableMethod(rpt, &ThreadUtilsObject::Test0);
  r1->Run();
  EXPECT_EQ(count += 1, rpt->mCount);

  static_assert(
      mozilla::IsSame< ::detail::ParameterStorage<int>::Type,
                      StoreCopyPassByConstLRef<int>>::value,
      "detail::ParameterStorage<int>::Type should be StoreCopyPassByConstLRef<int>");
  static_assert(
      mozilla::IsSame< ::detail::ParameterStorage<StoreCopyPassByValue<int>>::Type,
                      StoreCopyPassByValue<int>>::value,
      "detail::ParameterStorage<StoreCopyPassByValue<int>>::Type should be StoreCopyPassByValue<int>");

  r1 = NewRunnableMethod<int>(rpt, &ThreadUtilsObject::Test1i, 12);
  r1->Run();
  EXPECT_EQ(count += 2, rpt->mCount);
  EXPECT_EQ(12, rpt->mA0);

  r1 = NewRunnableMethod<int, int>(
       rpt, &ThreadUtilsObject::Test2i, 21, 22);
  r1->Run();
  EXPECT_EQ(count += 3, rpt->mCount);
  EXPECT_EQ(21, rpt->mA0);
  EXPECT_EQ(22, rpt->mA1);

  r1 = NewRunnableMethod<int, int, int>(
       rpt, &ThreadUtilsObject::Test3i, 31, 32, 33);
  r1->Run();
  EXPECT_EQ(count += 4, rpt->mCount);
  EXPECT_EQ(31, rpt->mA0);
  EXPECT_EQ(32, rpt->mA1);
  EXPECT_EQ(33, rpt->mA2);

  r1 = NewRunnableMethod<int, int, int, int>(
       rpt, &ThreadUtilsObject::Test4i, 41, 42, 43, 44);
  r1->Run();
  EXPECT_EQ(count += 5, rpt->mCount);
  EXPECT_EQ(41, rpt->mA0);
  EXPECT_EQ(42, rpt->mA1);
  EXPECT_EQ(43, rpt->mA2);
  EXPECT_EQ(44, rpt->mA3);

  // More interesting types of arguments.

  // Passing a short to make sure forwarding works with an inexact type match.
  short int si = 11;
  r1 = NewRunnableMethod<int>(rpt, &ThreadUtilsObject::Test1i, si);
  r1->Run();
  EXPECT_EQ(count += 2, rpt->mCount);
  EXPECT_EQ(si, rpt->mA0);

  // Raw pointer, possible cv-qualified.
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<int*>::Type,
                                StorePtrPassByPtr<int>>::value,
                "detail::ParameterStorage<int*>::Type should be StorePtrPassByPtr<int>");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<int* const>::Type,
                                StorePtrPassByPtr<int>>::value,
                "detail::ParameterStorage<int* const>::Type should be StorePtrPassByPtr<int>");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<int* volatile>::Type,
                                StorePtrPassByPtr<int>>::value,
                "detail::ParameterStorage<int* volatile>::Type should be StorePtrPassByPtr<int>");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<int* const volatile>::Type,
                                StorePtrPassByPtr<int>>::value,
                "detail::ParameterStorage<int* const volatile>::Type should be StorePtrPassByPtr<int>");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<int*>::Type::stored_type,
                                int*>::value,
                "detail::ParameterStorage<int*>::Type::stored_type should be int*");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<int*>::Type::passed_type,
                                int*>::value,
                "detail::ParameterStorage<int*>::Type::passed_type should be int*");
  {
    int i = 12;
    r1 = NewRunnableMethod<int*>(rpt, &ThreadUtilsObject::Test1pi, &i);
    r1->Run();
    EXPECT_EQ(count += 2, rpt->mCount);
    EXPECT_EQ(i, rpt->mA0);
  }

  // Raw pointer to const.
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<const int*>::Type,
                                StoreConstPtrPassByConstPtr<int>>::value,
                "detail::ParameterStorage<const int*>::Type should be StoreConstPtrPassByConstPtr<int>");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<const int* const>::Type,
                                StoreConstPtrPassByConstPtr<int>>::value,
                "detail::ParameterStorage<const int* const>::Type should be StoreConstPtrPassByConstPtr<int>");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<const int* volatile>::Type,
                                StoreConstPtrPassByConstPtr<int>>::value,
                "detail::ParameterStorage<const int* volatile>::Type should be StoreConstPtrPassByConstPtr<int>");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<const int* const volatile>::Type,
                                StoreConstPtrPassByConstPtr<int>>::value,
                "detail::ParameterStorage<const int* const volatile>::Type should be StoreConstPtrPassByConstPtr<int>");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<const int*>::Type::stored_type,
                                const int*>::value,
                "detail::ParameterStorage<const int*>::Type::stored_type should be const int*");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<const int*>::Type::passed_type,
                                const int*>::value,
                "detail::ParameterStorage<const int*>::Type::passed_type should be const int*");
  {
    int i = 1201;
    r1 = NewRunnableMethod<const int*>(rpt, &ThreadUtilsObject::Test1pci, &i);
    r1->Run();
    EXPECT_EQ(count += 2, rpt->mCount);
    EXPECT_EQ(i, rpt->mA0);
  }

  // Raw pointer to copy.
  static_assert(mozilla::IsSame<StoreCopyPassByPtr<int>::stored_type,
                                int>::value,
                "StoreCopyPassByPtr<int>::stored_type should be int");
  static_assert(mozilla::IsSame<StoreCopyPassByPtr<int>::passed_type,
                                int*>::value,
                "StoreCopyPassByPtr<int>::passed_type should be int*");
  {
    int i = 1202;
    r1 = NewRunnableMethod<StoreCopyPassByPtr<int>>(
         rpt, &ThreadUtilsObject::Test1pi, i);
    r1->Run();
    EXPECT_EQ(count += 2, rpt->mCount);
    EXPECT_EQ(i, rpt->mA0);
  }

  // Raw pointer to const copy.
  static_assert(mozilla::IsSame<StoreCopyPassByConstPtr<int>::stored_type,
                                int>::value,
                "StoreCopyPassByConstPtr<int>::stored_type should be int");
  static_assert(mozilla::IsSame<StoreCopyPassByConstPtr<int>::passed_type,
                                const int*>::value,
                "StoreCopyPassByConstPtr<int>::passed_type should be const int*");
  {
    int i = 1203;
    r1 = NewRunnableMethod<StoreCopyPassByConstPtr<int>>(
         rpt, &ThreadUtilsObject::Test1pci, i);
    r1->Run();
    EXPECT_EQ(count += 2, rpt->mCount);
    EXPECT_EQ(i, rpt->mA0);
  }

  // nsRefPtr to pointer.
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<StoreRefPtrPassByPtr<SpyWithISupports>>::Type,
                                StoreRefPtrPassByPtr<SpyWithISupports>>::value,
                "ParameterStorage<StoreRefPtrPassByPtr<SpyWithISupports>>::Type should be StoreRefPtrPassByPtr<SpyWithISupports>");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<SpyWithISupports*>::Type,
                                StoreRefPtrPassByPtr<SpyWithISupports>>::value,
                "ParameterStorage<SpyWithISupports*>::Type should be StoreRefPtrPassByPtr<SpyWithISupports>");
  static_assert(mozilla::IsSame<StoreRefPtrPassByPtr<SpyWithISupports>::stored_type,
                                RefPtr<SpyWithISupports>>::value,
                "StoreRefPtrPassByPtr<SpyWithISupports>::stored_type should be RefPtr<SpyWithISupports>");
  static_assert(mozilla::IsSame<StoreRefPtrPassByPtr<SpyWithISupports>::passed_type,
                                SpyWithISupports*>::value,
                "StoreRefPtrPassByPtr<SpyWithISupports>::passed_type should be SpyWithISupports*");
  // (more nsRefPtr tests below)

  // nsRefPtr for ref-countable classes that do not derive from ISupports.
  static_assert(::detail::HasRefCountMethods<ThreadUtilsRefCountedFinal>::value,
                "ThreadUtilsRefCountedFinal has AddRef() and Release()");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<ThreadUtilsRefCountedFinal*>::Type,
                                StoreRefPtrPassByPtr<ThreadUtilsRefCountedFinal>>::value,
                "ParameterStorage<ThreadUtilsRefCountedFinal*>::Type should be StoreRefPtrPassByPtr<ThreadUtilsRefCountedFinal>");
  static_assert(::detail::HasRefCountMethods<ThreadUtilsRefCountedBase>::value,
                "ThreadUtilsRefCountedBase has AddRef() and Release()");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<ThreadUtilsRefCountedBase*>::Type,
                                StoreRefPtrPassByPtr<ThreadUtilsRefCountedBase>>::value,
                "ParameterStorage<ThreadUtilsRefCountedBase*>::Type should be StoreRefPtrPassByPtr<ThreadUtilsRefCountedBase>");
  static_assert(::detail::HasRefCountMethods<ThreadUtilsRefCountedDerived>::value,
                "ThreadUtilsRefCountedDerived has AddRef() and Release()");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<ThreadUtilsRefCountedDerived*>::Type,
                                StoreRefPtrPassByPtr<ThreadUtilsRefCountedDerived>>::value,
                "ParameterStorage<ThreadUtilsRefCountedDerived*>::Type should be StoreRefPtrPassByPtr<ThreadUtilsRefCountedDerived>");

  static_assert(!::detail::HasRefCountMethods<ThreadUtilsNonRefCounted>::value,
                "ThreadUtilsNonRefCounted doesn't have AddRef() and Release()");
  static_assert(!mozilla::IsSame< ::detail::ParameterStorage<ThreadUtilsNonRefCounted*>::Type,
                                 StoreRefPtrPassByPtr<ThreadUtilsNonRefCounted>>::value,
                "ParameterStorage<ThreadUtilsNonRefCounted*>::Type should NOT be StoreRefPtrPassByPtr<ThreadUtilsNonRefCounted>");

  // Lvalue reference.
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<int&>::Type,
                                StoreRefPassByLRef<int>>::value,
                "ParameterStorage<int&>::Type should be StoreRefPassByLRef<int>");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<int&>::Type::stored_type,
                                StoreRefPassByLRef<int>::stored_type>::value,
                "ParameterStorage<int&>::Type::stored_type should be StoreRefPassByLRef<int>::stored_type");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<int&>::Type::stored_type,
                                int&>::value,
                "ParameterStorage<int&>::Type::stored_type should be int&");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<int&>::Type::passed_type,
                                int&>::value,
                "ParameterStorage<int&>::Type::passed_type should be int&");
  {
    int i = 13;
    r1 = NewRunnableMethod<int&>(rpt, &ThreadUtilsObject::Test1ri, i);
    r1->Run();
    EXPECT_EQ(count += 2, rpt->mCount);
    EXPECT_EQ(i, rpt->mA0);
  }

  // Rvalue reference -- Actually storing a copy and then moving it.
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<int&&>::Type,
                                StoreCopyPassByRRef<int>>::value,
                "ParameterStorage<int&&>::Type should be StoreCopyPassByRRef<int>");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<int&&>::Type::stored_type,
                                StoreCopyPassByRRef<int>::stored_type>::value,
                "ParameterStorage<int&&>::Type::stored_type should be StoreCopyPassByRRef<int>::stored_type");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<int&&>::Type::stored_type,
                                int>::value,
                "ParameterStorage<int&&>::Type::stored_type should be int");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<int&&>::Type::passed_type,
                                int&&>::value,
                "ParameterStorage<int&&>::Type::passed_type should be int&&");
  {
    int i = 14;
    r1 = NewRunnableMethod<int&&>(
          rpt, &ThreadUtilsObject::Test1rri, mozilla::Move(i));
  }
  r1->Run();
  EXPECT_EQ(count += 2, rpt->mCount);
  EXPECT_EQ(14, rpt->mA0);

  // Null unique pointer, by semi-implicit store&move with "T&&" syntax.
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<mozilla::UniquePtr<int>&&>::Type,
                                StoreCopyPassByRRef<mozilla::UniquePtr<int>>>::value,
                "ParameterStorage<UniquePtr<int>&&>::Type should be StoreCopyPassByRRef<UniquePtr<int>>");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<mozilla::UniquePtr<int>&&>::Type::stored_type,
                                StoreCopyPassByRRef<mozilla::UniquePtr<int>>::stored_type>::value,
                "ParameterStorage<UniquePtr<int>&&>::Type::stored_type should be StoreCopyPassByRRef<UniquePtr<int>>::stored_type");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<mozilla::UniquePtr<int>&&>::Type::stored_type,
                                mozilla::UniquePtr<int>>::value,
                "ParameterStorage<UniquePtr<int>&&>::Type::stored_type should be UniquePtr<int>");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<mozilla::UniquePtr<int>&&>::Type::passed_type,
                                mozilla::UniquePtr<int>&&>::value,
                "ParameterStorage<UniquePtr<int>&&>::Type::passed_type should be UniquePtr<int>&&");
  {
    mozilla::UniquePtr<int> upi;
    r1 = NewRunnableMethod<mozilla::UniquePtr<int>&&>(
          rpt, &ThreadUtilsObject::Test1upi, mozilla::Move(upi));
  }
  r1->Run();
  EXPECT_EQ(count += 2, rpt->mCount);
  EXPECT_EQ(-1, rpt->mA0);
  rpt->mA0 = 0;

  // Null unique pointer, by explicit store&move with "StoreCopyPassByRRef<T>" syntax.
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<StoreCopyPassByRRef<mozilla::UniquePtr<int>>>::Type::stored_type,
                                StoreCopyPassByRRef<mozilla::UniquePtr<int>>::stored_type>::value,
                "ParameterStorage<StoreCopyPassByRRef<UniquePtr<int>>>::Type::stored_type should be StoreCopyPassByRRef<UniquePtr<int>>::stored_type");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<StoreCopyPassByRRef<mozilla::UniquePtr<int>>>::Type::stored_type,
                                StoreCopyPassByRRef<mozilla::UniquePtr<int>>::stored_type>::value,
                "ParameterStorage<StoreCopyPassByRRef<UniquePtr<int>>>::Type::stored_type should be StoreCopyPassByRRef<UniquePtr<int>>::stored_type");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<StoreCopyPassByRRef<mozilla::UniquePtr<int>>>::Type::stored_type,
                                mozilla::UniquePtr<int>>::value,
                "ParameterStorage<StoreCopyPassByRRef<UniquePtr<int>>>::Type::stored_type should be UniquePtr<int>");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<StoreCopyPassByRRef<mozilla::UniquePtr<int>>>::Type::passed_type,
                                mozilla::UniquePtr<int>&&>::value,
                "ParameterStorage<StoreCopyPassByRRef<UniquePtr<int>>>::Type::passed_type should be UniquePtr<int>&&");
  {
    mozilla::UniquePtr<int> upi;
    r1 = NewRunnableMethod
         <StoreCopyPassByRRef<mozilla::UniquePtr<int>>>(
           rpt, &ThreadUtilsObject::Test1upi, mozilla::Move(upi));
  }
  r1->Run();
  EXPECT_EQ(count += 2, rpt->mCount);
  EXPECT_EQ(-1, rpt->mA0);

  // Unique pointer as xvalue.
  {
    mozilla::UniquePtr<int> upi = mozilla::MakeUnique<int>(1);
    r1 = NewRunnableMethod<mozilla::UniquePtr<int>&&>(
           rpt, &ThreadUtilsObject::Test1upi, mozilla::Move(upi));
  }
  r1->Run();
  EXPECT_EQ(count += 2, rpt->mCount);
  EXPECT_EQ(1, rpt->mA0);

  {
    mozilla::UniquePtr<int> upi = mozilla::MakeUnique<int>(1);
    r1 = NewRunnableMethod
         <StoreCopyPassByRRef<mozilla::UniquePtr<int>>>
         (rpt, &ThreadUtilsObject::Test1upi, mozilla::Move(upi));
  }
  r1->Run();
  EXPECT_EQ(count += 2, rpt->mCount);
  EXPECT_EQ(1, rpt->mA0);

  // Unique pointer as prvalue.
  r1 = NewRunnableMethod<mozilla::UniquePtr<int>&&>(
         rpt, &ThreadUtilsObject::Test1upi, mozilla::MakeUnique<int>(2));
  r1->Run();
  EXPECT_EQ(count += 2, rpt->mCount);
  EXPECT_EQ(2, rpt->mA0);

  // Unique pointer as lvalue to lref.
  {
    mozilla::UniquePtr<int> upi;
    r1 = NewRunnableMethod<mozilla::UniquePtr<int>&>(
           rpt, &ThreadUtilsObject::Test1rupi, upi);
    // Passed as lref, so Run() must be called while local upi is still alive!
    r1->Run();
  }
  EXPECT_EQ(count += 2, rpt->mCount);
  EXPECT_EQ(-1, rpt->mA0);

  // Verify copy/move assumptions.

  Spy::ClearAll();
  if (gDebug) { printf("%d - Test: Store copy from lvalue, pass by value\n", __LINE__); }
  { // Block around nsCOMPtr lifetime.
    nsCOMPtr<nsIRunnable> r2;
    { // Block around Spy lifetime.
      if (gDebug) { printf("%d - Spy s(10)\n", __LINE__); }
      Spy s(10);
      EXPECT_EQ(1, gConstructions);
      EXPECT_EQ(1, gAlive);
      if (gDebug) { printf("%d - r2 = NewRunnableMethod<StoreCopyPassByValue<Spy>>(&TestByValue, s)\n", __LINE__); }
      r2 = NewRunnableMethod<StoreCopyPassByValue<Spy>>(
           rpt, &ThreadUtilsObject::TestByValue, s);
      EXPECT_EQ(2, gAlive);
      EXPECT_LE(1, gCopyConstructions); // At least 1 copy-construction.
      Spy::ClearActions();
      if (gDebug) { printf("%d - End block with Spy s(10)\n", __LINE__); }
    }
    EXPECT_EQ(1, gDestructions);
    EXPECT_EQ(1, gAlive);
    Spy::ClearActions();
    if (gDebug) { printf("%d - Run()\n", __LINE__); }
    r2->Run();
    EXPECT_LE(1, gCopyConstructions); // Another copy-construction in call.
    EXPECT_EQ(10, rpt->mSpy.mID);
    EXPECT_LE(1, gDestructions);
    EXPECT_EQ(1, gAlive);
    Spy::ClearActions();
    if (gDebug) { printf("%d - End block with r\n", __LINE__); }
  }
  if (gDebug) { printf("%d - After end block with r\n", __LINE__); }
  EXPECT_EQ(1, gDestructions);
  EXPECT_EQ(0, gAlive);

  Spy::ClearAll();
  if (gDebug) { printf("%d - Test: Store copy from prvalue, pass by value\n", __LINE__); }
  {
    if (gDebug) { printf("%d - r3 = NewRunnableMethod<StoreCopyPassByValue<Spy>>(&TestByValue, Spy(11))\n", __LINE__); }
    nsCOMPtr<nsIRunnable> r3 =
      NewRunnableMethod<StoreCopyPassByValue<Spy>>(
        rpt, &ThreadUtilsObject::TestByValue, Spy(11));
    EXPECT_EQ(1, gAlive);
    EXPECT_EQ(1, gConstructions);
    EXPECT_LE(1, gMoveConstructions);
    Spy::ClearActions();
    if (gDebug) { printf("%d - Run()\n", __LINE__); }
    r3->Run();
    EXPECT_LE(1, gCopyConstructions); // Another copy-construction in call.
    EXPECT_EQ(11, rpt->mSpy.mID);
    EXPECT_LE(1, gDestructions);
    EXPECT_EQ(1, gAlive);
    Spy::ClearActions();
    if (gDebug) { printf("%d - End block with r\n", __LINE__); }
  }
  if (gDebug) { printf("%d - After end block with r\n", __LINE__); }
  EXPECT_EQ(1, gDestructions);
  EXPECT_EQ(0, gAlive);

  Spy::ClearAll();
  { // Store copy from xvalue, pass by value.
    nsCOMPtr<nsIRunnable> r4;
    {
      Spy s(12);
      EXPECT_EQ(1, gConstructions);
      EXPECT_EQ(1, gAlive);
      Spy::ClearActions();
      r4 = NewRunnableMethod<StoreCopyPassByValue<Spy>>(
          rpt, &ThreadUtilsObject::TestByValue, mozilla::Move(s));
      EXPECT_LE(1, gMoveConstructions);
      EXPECT_EQ(1, gAlive);
      EXPECT_EQ(1, gZombies);
      Spy::ClearActions();
    }
    EXPECT_EQ(1, gDestructions);
    EXPECT_EQ(1, gAlive);
    EXPECT_EQ(0, gZombies);
    Spy::ClearActions();
    r4->Run();
    EXPECT_LE(1, gCopyConstructions); // Another copy-construction in call.
    EXPECT_EQ(12, rpt->mSpy.mID);
    EXPECT_LE(1, gDestructions);
    EXPECT_EQ(1, gAlive);
    Spy::ClearActions();
  }
  EXPECT_EQ(1, gDestructions);
  EXPECT_EQ(0, gAlive);
  // Won't test xvalues anymore, prvalues are enough to verify all rvalues.

  Spy::ClearAll();
  if (gDebug) { printf("%d - Test: Store copy from lvalue, pass by const lvalue ref\n", __LINE__); }
  { // Block around nsCOMPtr lifetime.
    nsCOMPtr<nsIRunnable> r5;
    { // Block around Spy lifetime.
      if (gDebug) { printf("%d - Spy s(20)\n", __LINE__); }
      Spy s(20);
      EXPECT_EQ(1, gConstructions);
      EXPECT_EQ(1, gAlive);
      if (gDebug) { printf("%d - r5 = NewRunnableMethod<StoreCopyPassByConstLRef<Spy>>(&TestByConstLRef, s)\n", __LINE__); }
      r5 = NewRunnableMethod<StoreCopyPassByConstLRef<Spy>>(
           rpt, &ThreadUtilsObject::TestByConstLRef, s);
      EXPECT_EQ(2, gAlive);
      EXPECT_LE(1, gCopyConstructions); // At least 1 copy-construction.
      Spy::ClearActions();
      if (gDebug) { printf("%d - End block with Spy s(20)\n", __LINE__); }
    }
    EXPECT_EQ(1, gDestructions);
    EXPECT_EQ(1, gAlive);
    Spy::ClearActions();
    if (gDebug) { printf("%d - Run()\n", __LINE__); }
    r5->Run();
    EXPECT_EQ(0, gCopyConstructions); // No copies in call.
    EXPECT_EQ(20, rpt->mSpy.mID);
    EXPECT_EQ(0, gDestructions);
    EXPECT_EQ(1, gAlive);
    Spy::ClearActions();
    if (gDebug) { printf("%d - End block with r\n", __LINE__); }
  }
  if (gDebug) { printf("%d - After end block with r\n", __LINE__); }
  EXPECT_EQ(1, gDestructions);
  EXPECT_EQ(0, gAlive);

  Spy::ClearAll();
  if (gDebug) { printf("%d - Test: Store copy from prvalue, pass by const lvalue ref\n", __LINE__); }
  {
    if (gDebug) { printf("%d - r6 = NewRunnableMethod<StoreCopyPassByConstLRef<Spy>>(&TestByConstLRef, Spy(21))\n", __LINE__); }
    nsCOMPtr<nsIRunnable> r6 =
      NewRunnableMethod<StoreCopyPassByConstLRef<Spy>>(
        rpt, &ThreadUtilsObject::TestByConstLRef, Spy(21));
    EXPECT_EQ(1, gAlive);
    EXPECT_EQ(1, gConstructions);
    EXPECT_LE(1, gMoveConstructions);
    Spy::ClearActions();
    if (gDebug) { printf("%d - Run()\n", __LINE__); }
    r6->Run();
    EXPECT_EQ(0, gCopyConstructions); // No copies in call.
    EXPECT_EQ(21, rpt->mSpy.mID);
    EXPECT_EQ(0, gDestructions);
    EXPECT_EQ(1, gAlive);
    Spy::ClearActions();
    if (gDebug) { printf("%d - End block with r\n", __LINE__); }
  }
  if (gDebug) { printf("%d - After end block with r\n", __LINE__); }
  EXPECT_EQ(1, gDestructions);
  EXPECT_EQ(0, gAlive);

  Spy::ClearAll();
  if (gDebug) { printf("%d - Test: Store copy from lvalue, pass by rvalue ref\n", __LINE__); }
  { // Block around nsCOMPtr lifetime.
    nsCOMPtr<nsIRunnable> r7;
    { // Block around Spy lifetime.
      if (gDebug) { printf("%d - Spy s(30)\n", __LINE__); }
      Spy s(30);
      EXPECT_EQ(1, gConstructions);
      EXPECT_EQ(1, gAlive);
      if (gDebug) { printf("%d - r7 = NewRunnableMethod<StoreCopyPassByRRef<Spy>>(&TestByRRef, s)\n", __LINE__); }
      r7 = NewRunnableMethod<StoreCopyPassByRRef<Spy>>(
           rpt, &ThreadUtilsObject::TestByRRef, s);
      EXPECT_EQ(2, gAlive);
      EXPECT_LE(1, gCopyConstructions); // At least 1 copy-construction.
      Spy::ClearActions();
      if (gDebug) { printf("%d - End block with Spy s(30)\n", __LINE__); }
    }
    EXPECT_EQ(1, gDestructions);
    EXPECT_EQ(1, gAlive);
    Spy::ClearActions();
    if (gDebug) { printf("%d - Run()\n", __LINE__); }
    r7->Run();
    EXPECT_LE(1, gMoves); // Move in call.
    EXPECT_EQ(30, rpt->mSpy.mID);
    EXPECT_EQ(0, gDestructions);
    EXPECT_EQ(0, gAlive); // Spy inside Test is not counted.
    EXPECT_EQ(1, gZombies); // Our local spy should now be a zombie.
    Spy::ClearActions();
    if (gDebug) { printf("%d - End block with r\n", __LINE__); }
  }
  if (gDebug) { printf("%d - After end block with r\n", __LINE__); }
  EXPECT_EQ(1, gDestructions);
  EXPECT_EQ(0, gAlive);

  Spy::ClearAll();
  if (gDebug) { printf("%d - Test: Store copy from prvalue, pass by rvalue ref\n", __LINE__); }
  {
    if (gDebug) { printf("%d - r8 = NewRunnableMethod<StoreCopyPassByRRef<Spy>>(&TestByRRef, Spy(31))\n", __LINE__); }
    nsCOMPtr<nsIRunnable> r8 =
      NewRunnableMethod<StoreCopyPassByRRef<Spy>>(
        rpt, &ThreadUtilsObject::TestByRRef, Spy(31));
    EXPECT_EQ(1, gAlive);
    EXPECT_EQ(1, gConstructions);
    EXPECT_LE(1, gMoveConstructions);
    Spy::ClearActions();
    if (gDebug) { printf("%d - Run()\n", __LINE__); }
    r8->Run();
    EXPECT_LE(1, gMoves); // Move in call.
    EXPECT_EQ(31, rpt->mSpy.mID);
    EXPECT_EQ(0, gDestructions);
    EXPECT_EQ(0, gAlive); // Spy inside Test is not counted.
    EXPECT_EQ(1, gZombies); // Our local spy should now be a zombie.
    Spy::ClearActions();
    if (gDebug) { printf("%d - End block with r\n", __LINE__); }
  }
  if (gDebug) { printf("%d - After end block with r\n", __LINE__); }
  EXPECT_EQ(1, gDestructions);
  EXPECT_EQ(0, gAlive);

  Spy::ClearAll();
  if (gDebug) { printf("%d - Test: Store lvalue ref, pass lvalue ref\n", __LINE__); }
  {
    if (gDebug) { printf("%d - Spy s(40)\n", __LINE__); }
    Spy s(40);
    EXPECT_EQ(1, gConstructions);
    EXPECT_EQ(1, gAlive);
    Spy::ClearActions();
    if (gDebug) { printf("%d - r9 = NewRunnableMethod<Spy&>(&TestByLRef, s)\n", __LINE__); }
    nsCOMPtr<nsIRunnable> r9 =
      NewRunnableMethod<Spy&>(
        rpt, &ThreadUtilsObject::TestByLRef, s);
    EXPECT_EQ(0, gAllConstructions);
    EXPECT_EQ(0, gDestructions);
    EXPECT_EQ(1, gAlive);
    Spy::ClearActions();
    if (gDebug) { printf("%d - Run()\n", __LINE__); }
    r9->Run();
    EXPECT_LE(1, gAssignments); // Assignment from reference in call.
    EXPECT_EQ(40, rpt->mSpy.mID);
    EXPECT_EQ(&s, rpt->mSpyPtr);
    EXPECT_EQ(0, gDestructions);
    EXPECT_EQ(1, gAlive); // Spy inside Test is not counted.
    Spy::ClearActions();
    if (gDebug) { printf("%d - End block with r\n", __LINE__); }
  }
  if (gDebug) { printf("%d - After end block with r\n", __LINE__); }
  EXPECT_EQ(1, gDestructions);
  EXPECT_EQ(0, gAlive);

  Spy::ClearAll();
  if (gDebug) { printf("%d - Test: Store nsRefPtr, pass by pointer\n", __LINE__); }
  { // Block around nsCOMPtr lifetime.
    nsCOMPtr<nsIRunnable> r10;
    SpyWithISupports* ptr = 0;
    { // Block around RefPtr<Spy> lifetime.
      if (gDebug) { printf("%d - RefPtr<SpyWithISupports> s(new SpyWithISupports(45))\n", __LINE__); }
      RefPtr<SpyWithISupports> s(new SpyWithISupports(45));
      ptr = s.get();
      EXPECT_EQ(1, gConstructions);
      EXPECT_EQ(1, gAlive);
      if (gDebug) { printf("%d - r10 = NewRunnableMethod<StoreRefPtrPassByPtr<Spy>>(&TestByRRef, s.get())\n", __LINE__); }
      r10 = NewRunnableMethod<StoreRefPtrPassByPtr<SpyWithISupports>>(
            rpt, &ThreadUtilsObject::TestByPointer, s.get());
      EXPECT_LE(0, gAllConstructions);
      EXPECT_EQ(1, gAlive);
      Spy::ClearActions();
      if (gDebug) { printf("%d - End block with RefPtr<Spy> s\n", __LINE__); }
    }
    EXPECT_EQ(0, gDestructions);
    EXPECT_EQ(1, gAlive);
    Spy::ClearActions();
    if (gDebug) { printf("%d - Run()\n", __LINE__); }
    r10->Run();
    EXPECT_LE(1, gAssignments); // Assignment from pointee in call.
    EXPECT_EQ(45, rpt->mSpy.mID);
    EXPECT_EQ(ptr, rpt->mSpyPtr);
    EXPECT_EQ(0, gDestructions);
    EXPECT_EQ(1, gAlive); // Spy inside Test is not counted.
    Spy::ClearActions();
    if (gDebug) { printf("%d - End block with r\n", __LINE__); }
  }
  if (gDebug) { printf("%d - After end block with r\n", __LINE__); }
  EXPECT_EQ(1, gDestructions);
  EXPECT_EQ(0, gAlive);

  Spy::ClearAll();
  if (gDebug) { printf("%d - Test: Store pointer to lvalue, pass by pointer\n", __LINE__); }
  {
    if (gDebug) { printf("%d - Spy s(55)\n", __LINE__); }
    Spy s(55);
    EXPECT_EQ(1, gConstructions);
    EXPECT_EQ(1, gAlive);
    Spy::ClearActions();
    if (gDebug) { printf("%d - r11 = NewRunnableMethod<Spy*>(&TestByPointer, s)\n", __LINE__); }
    nsCOMPtr<nsIRunnable> r11 =
      NewRunnableMethod<Spy*>(
        rpt, &ThreadUtilsObject::TestByPointer, &s);
    EXPECT_EQ(0, gAllConstructions);
    EXPECT_EQ(0, gDestructions);
    EXPECT_EQ(1, gAlive);
    Spy::ClearActions();
    if (gDebug) { printf("%d - Run()\n", __LINE__); }
    r11->Run();
    EXPECT_LE(1, gAssignments); // Assignment from pointee in call.
    EXPECT_EQ(55, rpt->mSpy.mID);
    EXPECT_EQ(&s, rpt->mSpyPtr);
    EXPECT_EQ(0, gDestructions);
    EXPECT_EQ(1, gAlive); // Spy inside Test is not counted.
    Spy::ClearActions();
    if (gDebug) { printf("%d - End block with r\n", __LINE__); }
  }
  if (gDebug) { printf("%d - After end block with r\n", __LINE__); }
  EXPECT_EQ(1, gDestructions);
  EXPECT_EQ(0, gAlive);

  Spy::ClearAll();
  if (gDebug) { printf("%d - Test: Store pointer to const lvalue, pass by pointer\n", __LINE__); }
  {
    if (gDebug) { printf("%d - Spy s(60)\n", __LINE__); }
    Spy s(60);
    EXPECT_EQ(1, gConstructions);
    EXPECT_EQ(1, gAlive);
    Spy::ClearActions();
    if (gDebug) { printf("%d - r12 = NewRunnableMethod<Spy*>(&TestByPointer, s)\n", __LINE__); }
    nsCOMPtr<nsIRunnable> r12 =
      NewRunnableMethod<const Spy*>(
        rpt, &ThreadUtilsObject::TestByPointerToConst, &s);
    EXPECT_EQ(0, gAllConstructions);
    EXPECT_EQ(0, gDestructions);
    EXPECT_EQ(1, gAlive);
    Spy::ClearActions();
    if (gDebug) { printf("%d - Run()\n", __LINE__); }
    r12->Run();
    EXPECT_LE(1, gAssignments); // Assignment from pointee in call.
    EXPECT_EQ(60, rpt->mSpy.mID);
    EXPECT_EQ(&s, rpt->mSpyPtr);
    EXPECT_EQ(0, gDestructions);
    EXPECT_EQ(1, gAlive); // Spy inside Test is not counted.
    Spy::ClearActions();
    if (gDebug) { printf("%d - End block with r\n", __LINE__); }
  }
  if (gDebug) { printf("%d - After end block with r\n", __LINE__); }
  EXPECT_EQ(1, gDestructions);
  EXPECT_EQ(0, gAlive);
}
