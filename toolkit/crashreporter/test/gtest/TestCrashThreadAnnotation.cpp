/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ThreadAnnotation.h"

#include <string.h>

#include "gtest/gtest.h"
#include "mozilla/Monitor.h"
#include "mozilla/Unused.h"
#include "nsIThread.h"
#include "nsIRunnable.h"
#include "nsThreadUtils.h"

using mozilla::Monitor;
using mozilla::MonitorAutoLock;

namespace CrashReporter {
namespace {

TEST(TestCrashThreadAnnotation, TestInitShutdown)
{
  InitThreadAnnotation();
  ShutdownThreadAnnotation();
}

TEST(TestCrashThreadAnnotation, TestNestedInitShutdown)
{
  // No bad things should happen in case we have extra init/shutdown calls.
  InitThreadAnnotation();
  InitThreadAnnotation();
  ShutdownThreadAnnotation();
  ShutdownThreadAnnotation();
}

TEST(TestCrashThreadAnnotation, TestUnbalancedInit)
{
  // No bad things should happen in case we have unbalanced init/shutdown calls.
  InitThreadAnnotation();
  InitThreadAnnotation();
  ShutdownThreadAnnotation();
}

TEST(TestCrashThreadAnnotation, TestUnbalancedShutdown)
{
  // No bad things should happen in case we have unbalanced init/shutdown calls.
  InitThreadAnnotation();
  ShutdownThreadAnnotation();
  ShutdownThreadAnnotation();
}

TEST(TestCrashThreadAnnotation, TestGetFlatThreadAnnotation_BeforeInit)
{
  // GetFlatThreadAnnotation() should not return anything before init.
  std::function<void(const char*)> getThreadAnnotationCB =
        [&] (const char * aAnnotation) -> void {
    ASSERT_STREQ(aAnnotation, "");
  };
  GetFlatThreadAnnotation(getThreadAnnotationCB);
}

TEST(TestCrashThreadAnnotation, TestGetFlatThreadAnnotation_AfterShutdown)
{
  // GetFlatThreadAnnotation() should not return anything after shutdown.
  InitThreadAnnotation();
  ShutdownThreadAnnotation();

  std::function<void(const char*)> getThreadAnnotationCB =
        [&] (const char * aAnnotation) -> void {
    ASSERT_STREQ(aAnnotation, "");
  };
  GetFlatThreadAnnotation(getThreadAnnotationCB);
}

already_AddRefed<nsIThread>
CreateTestThread(const char* aName, Monitor& aMonitor, bool& aDone)
{
  nsCOMPtr<nsIRunnable> setNameRunnable = NS_NewRunnableFunction([aName, &aMonitor, &aDone] () -> void {
    NS_SetCurrentThreadName(aName);

    MonitorAutoLock lock(aMonitor);
    aDone = true;
    aMonitor.NotifyAll();
  });
  nsCOMPtr<nsIThread> thread;
  mozilla::Unused << NS_NewThread(getter_AddRefs(thread), setNameRunnable);

  return thread.forget();
}

TEST(TestCrashThreadAnnotation, TestGetFlatThreadAnnotation_OneThread)
{
  InitThreadAnnotation();

  Monitor monitor("TestCrashThreadAnnotation");
  bool threadNameSet = false;
  nsCOMPtr<nsIThread> thread = CreateTestThread("Thread1", monitor, threadNameSet);
  ASSERT_TRUE(!!thread);

  {
    MonitorAutoLock lock(monitor);
    while (!threadNameSet) {
      monitor.Wait();
    }
  }

  std::function<void(const char*)> getThreadAnnotationCB =
        [&] (const char * aAnnotation) -> void {
    ASSERT_TRUE(!!strstr(aAnnotation, "Thread1"));
  };
  GetFlatThreadAnnotation(getThreadAnnotationCB);

  ShutdownThreadAnnotation();

  thread->Shutdown();
}

TEST(TestCrashThreadAnnotation, TestGetFlatThreadAnnotation_SetNameTwice)
{
  InitThreadAnnotation();

  Monitor monitor("TestCrashThreadAnnotation");
  bool threadNameSet = false;

  nsCOMPtr<nsIRunnable> setNameRunnable = NS_NewRunnableFunction([&] () -> void {
    NS_SetCurrentThreadName("Thread1");
    // Set the name again. We should get the latest name.
    NS_SetCurrentThreadName("Thread1Again");

    MonitorAutoLock lock(monitor);
    threadNameSet = true;
    monitor.NotifyAll();
  });
  nsCOMPtr<nsIThread> thread;
  nsresult rv = NS_NewThread(getter_AddRefs(thread), setNameRunnable);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  {
    MonitorAutoLock lock(monitor);
    while (!threadNameSet) {
      monitor.Wait();
    }
  }

  std::function<void(const char*)> getThreadAnnotationCB =
        [&] (const char * aAnnotation) -> void {
    ASSERT_TRUE(!!strstr(aAnnotation, "Thread1Again"));
  };
  GetFlatThreadAnnotation(getThreadAnnotationCB);

  ShutdownThreadAnnotation();

  thread->Shutdown();
}

TEST(TestCrashThreadAnnotation, TestGetFlatThreadAnnotation_TwoThreads)
{
  InitThreadAnnotation();

  Monitor monitor("TestCrashThreadAnnotation");
  bool thread1NameSet = false;
  bool thread2NameSet = false;

  nsCOMPtr<nsIThread> thread1 = CreateTestThread("Thread1", monitor, thread1NameSet);
  ASSERT_TRUE(!!thread1);

  nsCOMPtr<nsIThread> thread2 = CreateTestThread("Thread2", monitor, thread2NameSet);
  ASSERT_TRUE(!!thread2);

  {
    MonitorAutoLock lock(monitor);
    while (!(thread1NameSet && thread2NameSet)) {
      monitor.Wait();
    }
  }

  std::function<void(const char*)> getThreadAnnotationCB =
        [&] (const char * aAnnotation) -> void {
    // Assert that Thread1 and Thread2 are both in the annotation data.
    ASSERT_TRUE(!!strstr(aAnnotation, "Thread1"));
    ASSERT_TRUE(!!strstr(aAnnotation, "Thread2"));
  };
  GetFlatThreadAnnotation(getThreadAnnotationCB);

  ShutdownThreadAnnotation();

  thread1->Shutdown();
  thread2->Shutdown();
}

TEST(TestCrashThreadAnnotation, TestGetFlatThreadAnnotation_ShutdownOneThread)
{
  InitThreadAnnotation();

  Monitor monitor("TestCrashThreadAnnotation");
  bool thread1NameSet = false;
  bool thread2NameSet = false;

  nsCOMPtr<nsIThread> thread1 = CreateTestThread("Thread1", monitor, thread1NameSet);
  ASSERT_TRUE(!!thread1);

  nsCOMPtr<nsIThread> thread2 = CreateTestThread("Thread2", monitor, thread2NameSet);
  ASSERT_TRUE(!!thread2);

  {
    MonitorAutoLock lock(monitor);
    while (!(thread1NameSet && thread2NameSet)) {
      monitor.Wait();
    }
  }

  nsresult rv = thread1->Shutdown();
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  std::function<void(const char*)> getThreadAnnotationCB =
        [&] (const char * aAnnotation) -> void {
    // Assert that only Thread2 is present in the annotation data.
    ASSERT_TRUE(!strstr(aAnnotation, "Thread1"));
    ASSERT_TRUE(!!strstr(aAnnotation, "Thread2"));
  };
  GetFlatThreadAnnotation(getThreadAnnotationCB);

  ShutdownThreadAnnotation();

  thread2->Shutdown();
}

TEST(TestCrashThreadAnnotation, TestGetFlatThreadAnnotation_ShutdownBothThreads)
{
  InitThreadAnnotation();

  Monitor monitor("TestCrashThreadAnnotation");
  bool thread1NameSet = false;
  bool thread2NameSet = false;

  nsCOMPtr<nsIThread> thread1 = CreateTestThread("Thread1", monitor, thread1NameSet);
  ASSERT_TRUE(!!thread1);

  nsCOMPtr<nsIThread> thread2 = CreateTestThread("Thread2", monitor, thread2NameSet);
  ASSERT_TRUE(!!thread2);

  {
    MonitorAutoLock lock(monitor);
    while (!(thread1NameSet && thread2NameSet)) {
      monitor.Wait();
    }
  }

  nsresult rv = thread1->Shutdown();
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  rv = thread2->Shutdown();
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  std::function<void(const char*)> getThreadAnnotationCB =
        [&] (const char * aAnnotation) -> void {
    // No leftover in annnotation data.
    ASSERT_STREQ(aAnnotation, "");
  };
  GetFlatThreadAnnotation(getThreadAnnotationCB);

  ShutdownThreadAnnotation();
}

} // Anonymous namespace.
} // namespace CrashReporter
