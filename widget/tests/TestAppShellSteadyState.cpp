/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "TestHarness.h"

#include "nsIAppShell.h"
#include "nsIAppShellService.h"
#include "nsIDocument.h"
#include "nsIDOMEvent.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMWindow.h"
#include "nsIDOMWindowUtils.h"
#include "nsIInterfaceRequestor.h"
#include "nsIRunnable.h"
#include "nsIURI.h"
#include "nsIWebBrowserChrome.h"
#include "nsIXULWindow.h"

#include "nsAppShellCID.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsNetUtil.h"
#include "nsThreadUtils.h"
#include "mozilla/Attributes.h"

#ifdef XP_WIN
#include <windows.h>
#endif

using namespace mozilla;

typedef void (*TestFunc)(nsIAppShell*);

bool gStableStateEventHasRun = false;

class ExitAppShellRunnable : public nsRunnable
{
  nsCOMPtr<nsIAppShell> mAppShell;

public:
  ExitAppShellRunnable(nsIAppShell* aAppShell)
  : mAppShell(aAppShell)
  { }

  NS_IMETHOD
  Run()
  {
    return mAppShell->Exit();
  }
};

class StableStateRunnable : public nsRunnable
{
public:
  NS_IMETHOD
  Run()
  {
    if (gStableStateEventHasRun) {
      fail("StableStateRunnable already ran");
    }
    gStableStateEventHasRun = true;
    return NS_OK;
  }
};

class CheckStableStateRunnable : public nsRunnable
{
  bool mShouldHaveRun;

public:
  CheckStableStateRunnable(bool aShouldHaveRun)
  : mShouldHaveRun(aShouldHaveRun)
  { }

  NS_IMETHOD
  Run()
  {
    if (mShouldHaveRun == gStableStateEventHasRun) {
      passed("StableStateRunnable state correct (%s)",
             mShouldHaveRun ? "true" : "false");
    } else {
      fail("StableStateRunnable ran at wrong time");
    }
    return NS_OK;
  }
};

class ScheduleStableStateRunnable : public CheckStableStateRunnable
{
protected:
  nsCOMPtr<nsIAppShell> mAppShell;

public:
  ScheduleStableStateRunnable(nsIAppShell* aAppShell)
  : CheckStableStateRunnable(false), mAppShell(aAppShell)
  { }

  NS_IMETHOD
  Run()
  {
    CheckStableStateRunnable::Run();

    nsCOMPtr<nsIRunnable> runnable = new StableStateRunnable();
    nsresult rv = mAppShell->RunBeforeNextEvent(runnable);
    if (NS_FAILED(rv)) {
      fail("RunBeforeNextEvent returned failure code %u", rv);
    }

    return rv;
  }
};

class NextTestRunnable : public nsRunnable
{
  nsCOMPtr<nsIAppShell> mAppShell;

public:
  NextTestRunnable(nsIAppShell* aAppShell)
  : mAppShell(aAppShell)
  { }

  NS_IMETHOD Run();
};

class ScheduleNestedStableStateRunnable : public ScheduleStableStateRunnable
{
public:
  ScheduleNestedStableStateRunnable(nsIAppShell* aAppShell)
  : ScheduleStableStateRunnable(aAppShell)
  { }

  NS_IMETHOD
  Run()
  {
    ScheduleStableStateRunnable::Run();

    nsCOMPtr<nsIRunnable> runnable = new CheckStableStateRunnable(false);
    if (NS_FAILED(NS_DispatchToCurrentThread(runnable))) {
      fail("Failed to dispatch check runnable");
    }

    if (NS_FAILED(NS_ProcessPendingEvents(nullptr))) {
      fail("Failed to process all pending events");
    }

    runnable = new CheckStableStateRunnable(true);
    if (NS_FAILED(NS_DispatchToCurrentThread(runnable))) {
      fail("Failed to dispatch check runnable");
    }

    runnable = new NextTestRunnable(mAppShell);
    if (NS_FAILED(NS_DispatchToCurrentThread(runnable))) {
      fail("Failed to dispatch next test runnable");
    }

    return NS_OK;
  }
};

class EventListener MOZ_FINAL : public nsIDOMEventListener
{
  nsCOMPtr<nsIAppShell> mAppShell;

  static nsIDOMWindowUtils* sWindowUtils;
  static nsIAppShell* sAppShell;

  ~EventListener() {}

public:
  NS_DECL_ISUPPORTS

  EventListener(nsIAppShell* aAppShell)
  : mAppShell(aAppShell)
  { }

  NS_IMETHOD
  HandleEvent(nsIDOMEvent* aEvent)
  {
    nsString type;
    if (NS_FAILED(aEvent->GetType(type))) {
      fail("Failed to get event type");
      return NS_ERROR_FAILURE;
    }

    if (type.EqualsLiteral("load")) {
      passed("Got load event");

      nsCOMPtr<nsIDOMEventTarget> target;
      if (NS_FAILED(aEvent->GetTarget(getter_AddRefs(target)))) {
        fail("Failed to get event type");
        return NS_ERROR_FAILURE;
      }

      nsCOMPtr<nsIDocument> document = do_QueryInterface(target);
      if (!document) {
        fail("Failed to QI to nsIDocument!");
        return NS_ERROR_FAILURE;
      }

      nsCOMPtr<nsPIDOMWindow> window = document->GetWindow();
      if (!window) {
        fail("Failed to get window from document!");
        return NS_ERROR_FAILURE;
      }

      nsCOMPtr<nsIDOMWindowUtils> utils = do_GetInterface(window);
      if (!utils) {
        fail("Failed to get DOMWindowUtils!");
        return NS_ERROR_FAILURE;
      }

      if (!ScheduleTimer(utils)) {
        return NS_ERROR_FAILURE;
      }

      return NS_OK;
    }

    if (type.EqualsLiteral("keypress")) {
      passed("Got keypress event");

      nsCOMPtr<nsIRunnable> runnable = new StableStateRunnable();
      nsresult rv = mAppShell->RunBeforeNextEvent(runnable);
      if (NS_FAILED(rv)) {
        fail("RunBeforeNextEvent returned failure code %u", rv);
        return NS_ERROR_FAILURE;
      }

      return NS_OK;
    }

    fail("Got an unexpected event: %s", NS_ConvertUTF16toUTF8(type).get());
    return NS_OK;
  }

#ifdef XP_WIN
  static VOID CALLBACK
  TimerCallback(HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime)
  {
    if (sWindowUtils) {
      nsCOMPtr<nsIDOMWindowUtils> utils = dont_AddRef(sWindowUtils);
      sWindowUtils = nullptr;

      if (gStableStateEventHasRun) {
        fail("StableStateRunnable ran at wrong time");
      } else {
        passed("StableStateRunnable state correct (false)");
      }

      int32_t layout = 0x409; // US
      int32_t keyCode = 0x41; // VK_A
      NS_NAMED_LITERAL_STRING(a, "a");

      if (NS_FAILED(utils->SendNativeKeyEvent(layout, keyCode, 0, a, a))) {
        fail("Failed to synthesize native event");
      }

      return;
    }

    KillTimer(nullptr, idEvent);

    nsCOMPtr<nsIAppShell> appShell = dont_AddRef(sAppShell);

    if (!gStableStateEventHasRun) {
      fail("StableStateRunnable didn't run yet");
    } else {
      passed("StableStateRunnable state correct (true)");
    }

    nsCOMPtr<nsIRunnable> runnable = new NextTestRunnable(appShell);
    if (NS_FAILED(NS_DispatchToCurrentThread(runnable))) {
      fail("Failed to dispatch next test runnable");
    }

  }
#endif

  bool
  ScheduleTimer(nsIDOMWindowUtils* aWindowUtils)
  {
#ifdef XP_WIN
    UINT_PTR timerId = SetTimer(nullptr, 0, 1000, (TIMERPROC)TimerCallback);
    if (!timerId) {
      fail("SetTimer failed!");
      return false;
    }

    nsCOMPtr<nsIDOMWindowUtils> utils = aWindowUtils;
    utils.forget(&sWindowUtils);

    nsCOMPtr<nsIAppShell> appShell = mAppShell;
    appShell.forget(&sAppShell);

    return true;
#else
    return false;
#endif
  }
};

nsIDOMWindowUtils* EventListener::sWindowUtils = nullptr;
nsIAppShell* EventListener::sAppShell = nullptr;

NS_IMPL_ISUPPORTS(EventListener, nsIDOMEventListener)

already_AddRefed<nsIAppShell>
GetAppShell()
{
  static const char* platforms[] = {
    "android", "mac", "gonk", "gtk", "qt", "win"
  };

  NS_NAMED_LITERAL_CSTRING(contractPrefix, "@mozilla.org/widget/appshell/");
  NS_NAMED_LITERAL_CSTRING(contractSuffix, ";1");

  for (size_t index = 0; index < ArrayLength(platforms); index++) {
    nsAutoCString contractID(contractPrefix);
    contractID.AppendASCII(platforms[index]);
    contractID.Append(contractSuffix);

    nsCOMPtr<nsIAppShell> appShell = do_GetService(contractID.get());
    if (appShell) {
      return appShell.forget();
    }
  }

  return nullptr;
}

void
Test1(nsIAppShell* aAppShell)
{
  // Schedule stable state runnable to be run before next event.

  nsCOMPtr<nsIRunnable> runnable = new StableStateRunnable();
  if (NS_FAILED(aAppShell->RunBeforeNextEvent(runnable))) {
    fail("RunBeforeNextEvent failed");
  }

  runnable = new CheckStableStateRunnable(true);
  if (NS_FAILED(NS_DispatchToCurrentThread(runnable))) {
    fail("Failed to dispatch check runnable");
  }

  runnable = new NextTestRunnable(aAppShell);
  if (NS_FAILED(NS_DispatchToCurrentThread(runnable))) {
    fail("Failed to dispatch next test runnable");
  }
}

void
Test2(nsIAppShell* aAppShell)
{
  // Schedule stable state runnable to be run before next event from another
  // runnable.

  nsCOMPtr<nsIRunnable> runnable = new ScheduleStableStateRunnable(aAppShell);
  if (NS_FAILED(NS_DispatchToCurrentThread(runnable))) {
    fail("Failed to dispatch schedule runnable");
  }

  runnable = new CheckStableStateRunnable(true);
  if (NS_FAILED(NS_DispatchToCurrentThread(runnable))) {
    fail("Failed to dispatch check runnable");
  }

  runnable = new NextTestRunnable(aAppShell);
  if (NS_FAILED(NS_DispatchToCurrentThread(runnable))) {
    fail("Failed to dispatch next test runnable");
  }
}

void
Test3(nsIAppShell* aAppShell)
{
  // Schedule steadystate runnable to be run before next event with nested loop.

  nsCOMPtr<nsIRunnable> runnable =
    new ScheduleNestedStableStateRunnable(aAppShell);
  if (NS_FAILED(NS_DispatchToCurrentThread(runnable))) {
    fail("Failed to dispatch schedule runnable");
  }
}

bool
Test4Internal(nsIAppShell* aAppShell)
{
#ifndef XP_WIN
  // Not sure how to test on other platforms.
  return false;
#endif

  nsCOMPtr<nsIAppShellService> appService =
    do_GetService(NS_APPSHELLSERVICE_CONTRACTID);
  if (!appService) {
    fail("Failed to get appshell service!");
    return false;
  }

  nsCOMPtr<nsIURI> uri;
  if (NS_FAILED(NS_NewURI(getter_AddRefs(uri), "about:", nullptr))) {
    fail("Failed to create new uri");
    return false;
  }

  uint32_t flags = nsIWebBrowserChrome::CHROME_DEFAULT;

  nsCOMPtr<nsIXULWindow> xulWindow;
  if (NS_FAILED(appService->CreateTopLevelWindow(nullptr, uri, flags, 100, 100, nullptr,
                                                 getter_AddRefs(xulWindow)))) {
    fail("Failed to create new window");
    return false;
  }

  nsCOMPtr<nsIDOMWindow> window = do_GetInterface(xulWindow);
  if (!window) {
    fail("Can't get dom window!");
    return false;
  }

  nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(window);
  if (!target) {
    fail("Can't QI to nsIDOMEventTarget!");
    return false;
  }

  nsCOMPtr<nsIDOMEventListener> listener = new EventListener(aAppShell);
  if (NS_FAILED(target->AddEventListener(NS_LITERAL_STRING("keypress"),
                                         listener, false, false)) ||
      NS_FAILED(target->AddEventListener(NS_LITERAL_STRING("load"), listener,
                                         false, false))) {
    fail("Can't add event listeners!");
    return false;
  }

  return true;
}

void
Test4(nsIAppShell* aAppShell)
{
  if (!Test4Internal(aAppShell)) {
    nsCOMPtr<nsIRunnable> runnable = new NextTestRunnable(aAppShell);
    if (NS_FAILED(NS_DispatchToCurrentThread(runnable))) {
      fail("Failed to dispatch next test runnable");
    }
  }
}

const TestFunc gTests[] = {
  Test1, Test2, Test3, Test4
};

size_t gTestIndex = 0;

NS_IMETHODIMP
NextTestRunnable::Run()
{
  if (gTestIndex > 0) {
    passed("Finished test %u", gTestIndex);
  }

  gStableStateEventHasRun = false;

  if (gTestIndex < ArrayLength(gTests)) {
    gTests[gTestIndex++](mAppShell);
  }
  else {
    nsCOMPtr<nsIRunnable> exitRunnable = new ExitAppShellRunnable(mAppShell);

    nsresult rv = NS_DispatchToCurrentThread(exitRunnable);
    if (NS_FAILED(rv)) {
      fail("Failed to dispatch exit runnable!");
    }
  }

  return NS_OK;
}

int main(int argc, char** argv)
{
  ScopedLogging log;
  ScopedXPCOM xpcom("TestAppShellSteadyState");

  if (!xpcom.failed()) {
    nsCOMPtr<nsIAppShell> appShell = GetAppShell();
    if (!appShell) {
      fail("Couldn't get appshell!");
    } else {
      nsCOMPtr<nsIRunnable> runnable = new NextTestRunnable(appShell);
      if (NS_FAILED(NS_DispatchToCurrentThread(runnable))) {
        fail("Failed to dispatch next test runnable");
      } else if (NS_FAILED(appShell->Run())) {
        fail("Failed to run appshell");
      }
    }
  }

  return gFailCount != 0;
}
