/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http:mozilla.org/MPL/2.0/. */

#include "TestHarness.h"
#include "nsThreadUtils.h"

enum {
  TEST_CALL_VOID_ARG_VOID_RETURN,
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
  virtual ~nsFoo() {}
};

namespace mozilla {
template<>
struct HasDangerousPublicDestructor<nsFoo>
{
  static const bool value = true;
};
}

NS_IMPL_ISUPPORTS0(nsFoo)

class TestSuicide : public nsRunnable {
  NS_IMETHOD Run() {
    // Runs first time on thread "Suicide", then dies on MainThread
    if (!NS_IsMainThread()) {
      mThread = do_GetCurrentThread();
      NS_DispatchToMainThread(this);
      return NS_OK;
    }
    MOZ_ASSERT(mThread);
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

int main(int argc, char** argv)
{
  ScopedXPCOM xpcom("ThreadUtils");
  NS_ENSURE_FALSE(xpcom.failed(), 1);

  memset(gRunnableExecuted, false, MAX_TESTS * sizeof(bool));
  // Scope the smart ptrs so that the runnables need to hold on to whatever they need
  {
    nsRefPtr<nsFoo> foo = new nsFoo();
    nsRefPtr<nsBar> bar = new nsBar();

    // This pointer will be freed at the end of the block
    // Do not dereference this pointer in the runnable method!
    nsFoo * rawFoo = new nsFoo();

    // Read only string. Dereferencing in runnable method to check this works.
    char* message = (char*)"Test message";

    NS_DispatchToMainThread(NS_NewRunnableMethod(bar, &nsBar::DoBar1));
    NS_DispatchToMainThread(NS_NewRunnableMethod(bar, &nsBar::DoBar2));
    NS_DispatchToMainThread(NS_NewRunnableMethodWithArg< nsRefPtr<nsFoo> >
      (bar, &nsBar::DoBar3, foo));
    NS_DispatchToMainThread(NS_NewRunnableMethodWithArg< nsRefPtr<nsFoo> >
      (bar, &nsBar::DoBar4, foo));
    NS_DispatchToMainThread(NS_NewRunnableMethodWithArg<nsFoo*>(bar, &nsBar::DoBar5, rawFoo));
    NS_DispatchToMainThread(NS_NewRunnableMethodWithArg<char*>(bar, &nsBar::DoBar6, message));
#ifdef HAVE_STDCALL
    NS_DispatchToMainThread(NS_NewRunnableMethod(bar, &nsBar::DoBar1std));
    NS_DispatchToMainThread(NS_NewRunnableMethod(bar, &nsBar::DoBar2std));
    NS_DispatchToMainThread(NS_NewRunnableMethodWithArg< nsRefPtr<nsFoo> >
      (bar, &nsBar::DoBar3std, foo));
    NS_DispatchToMainThread(NS_NewRunnableMethodWithArg< nsRefPtr<nsFoo> >
      (bar, &nsBar::DoBar4std, foo));
    NS_DispatchToMainThread(NS_NewRunnableMethodWithArg<nsFoo*>(bar, &nsBar::DoBar5std, rawFoo));
    NS_DispatchToMainThread(NS_NewRunnableMethodWithArg<char*>(bar, &nsBar::DoBar6std, message));
#endif

    delete rawFoo;
  }

  // Spin the event loop
  NS_ProcessPendingEvents(nullptr);

  // Now test a suicidal event in NS_New(Named)Thread
  nsCOMPtr<nsIThread> thread;
  NS_NewNamedThread("SuicideThread", getter_AddRefs(thread), new TestSuicide());
  MOZ_ASSERT(thread);

  while (!gRunnableExecuted[TEST_CALL_NEWTHREAD_SUICIDAL]) {
    NS_ProcessPendingEvents(nullptr);
  }

  int result = 0;

  for (uint32_t i = 0; i < MAX_TESTS; i++) {
    if (gRunnableExecuted[i]) {
      passed("Test %d passed",i);
    } else {
      fail("Error in test %d", i);
      result = 1;
    }
  }

  return result;
}
