/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Thread Pool Listener Test Code.
 *
 * The Initial Developer of the Original Code is
 *   Ben Turner <bent.mozilla@gmail.com>.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "TestHarness.h"

#include "nsIProxyObjectManager.h"
#include "nsIThread.h"
#include "nsIThreadPool.h"

#include "nsThreadUtils.h"
#include "nsXPCOMCIDInternal.h"
#include "pratom.h"
#include "prinrval.h"
#include "prmon.h"
#include "prthread.h"

#include "mozilla/ReentrantMonitor.h"
using namespace mozilla;

#define NUMBER_OF_THREADS 4

// One hour... because test boxes can be slow!
#define IDLE_THREAD_TIMEOUT 3600000

static nsIThread** gCreatedThreadList = nsnull;
static nsIThread** gShutDownThreadList = nsnull;

static ReentrantMonitor* gReentrantMonitor = nsnull;

static bool gAllRunnablesPosted = false;
static bool gAllThreadsCreated = false;
static bool gAllThreadsShutDown = false;

#ifdef DEBUG
#define TEST_ASSERTION(_test, _msg) \
    NS_ASSERTION(_test, _msg);
#else
#define TEST_ASSERTION(_test, _msg) \
  PR_BEGIN_MACRO \
    if (!(_test)) { \
      NS_DebugBreak(NS_DEBUG_ABORT, _msg, #_test, __FILE__, __LINE__); \
    } \
  PR_END_MACRO
#endif

class Listener : public nsIThreadPoolListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITHREADPOOLLISTENER
};

NS_IMPL_THREADSAFE_ISUPPORTS1(Listener, nsIThreadPoolListener)

NS_IMETHODIMP
Listener::OnThreadCreated()
{
  nsCOMPtr<nsIThread> current(do_GetCurrentThread());
  TEST_ASSERTION(current, "Couldn't get current thread!");

  ReentrantMonitorAutoEnter mon(*gReentrantMonitor);

  while (!gAllRunnablesPosted) {
    mon.Wait();
  }

  for (PRUint32 i = 0; i < NUMBER_OF_THREADS; i++) {
    nsIThread* thread = gCreatedThreadList[i];
    TEST_ASSERTION(thread != current, "Saw the same thread twice!");

    if (!thread) {
      gCreatedThreadList[i] = current;
      if (i == (NUMBER_OF_THREADS - 1)) {
        gAllThreadsCreated = PR_TRUE;
        mon.NotifyAll();
      }
      return NS_OK;
    }
  }

  TEST_ASSERTION(PR_FALSE, "Too many threads!");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
Listener::OnThreadShuttingDown()
{
  nsCOMPtr<nsIThread> current(do_GetCurrentThread());
  TEST_ASSERTION(current, "Couldn't get current thread!");

  ReentrantMonitorAutoEnter mon(*gReentrantMonitor);

  for (PRUint32 i = 0; i < NUMBER_OF_THREADS; i++) {
    nsIThread* thread = gShutDownThreadList[i];
    TEST_ASSERTION(thread != current, "Saw the same thread twice!");

    if (!thread) {
      gShutDownThreadList[i] = current;
      if (i == (NUMBER_OF_THREADS - 1)) {
        gAllThreadsShutDown = PR_TRUE;
        mon.NotifyAll();
      }
      return NS_OK;
    }
  }

  TEST_ASSERTION(PR_FALSE, "Too many threads!");
  return NS_ERROR_FAILURE;
}

class AutoCreateAndDestroyReentrantMonitor
{
public:
  AutoCreateAndDestroyReentrantMonitor(ReentrantMonitor** aReentrantMonitorPtr)
  : mReentrantMonitorPtr(aReentrantMonitorPtr) {
    *aReentrantMonitorPtr = new ReentrantMonitor("TestThreadPoolListener::AutoMon");
    TEST_ASSERTION(*aReentrantMonitorPtr, "Out of memory!");
  }

  ~AutoCreateAndDestroyReentrantMonitor() {
    if (*mReentrantMonitorPtr) {
      delete *mReentrantMonitorPtr;
      *mReentrantMonitorPtr = nsnull;
    }
  }

private:
  ReentrantMonitor** mReentrantMonitorPtr;
};

int main(int argc, char** argv)
{
  ScopedXPCOM xpcom("ThreadPoolListener");
  NS_ENSURE_FALSE(xpcom.failed(), 1);

  nsIThread* createdThreadList[NUMBER_OF_THREADS] = { nsnull };
  gCreatedThreadList = createdThreadList;

  nsIThread* shutDownThreadList[NUMBER_OF_THREADS] = { nsnull };
  gShutDownThreadList = shutDownThreadList;

  AutoCreateAndDestroyReentrantMonitor newMon(&gReentrantMonitor);
  NS_ENSURE_TRUE(gReentrantMonitor, 1);

  nsresult rv;

  // Grab the proxy service before messing with the thread pool. This is a
  // workaround for bug 449822 where the thread pool shutdown can create two
  // instances of the proxy service and hang.
  nsCOMPtr<nsIProxyObjectManager> proxyObjMgr =
    do_GetService(NS_XPCOMPROXY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIThreadPool> pool =
    do_CreateInstance(NS_THREADPOOL_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, 1);

  rv = pool->SetThreadLimit(NUMBER_OF_THREADS);
  NS_ENSURE_SUCCESS(rv, 1);

  rv = pool->SetIdleThreadLimit(NUMBER_OF_THREADS);
  NS_ENSURE_SUCCESS(rv, 1);

  rv = pool->SetIdleThreadTimeout(IDLE_THREAD_TIMEOUT);
  NS_ENSURE_SUCCESS(rv, 1);

  nsCOMPtr<nsIThreadPoolListener> listener = new Listener();
  NS_ENSURE_TRUE(listener, 1);

  rv = pool->SetListener(listener);
  NS_ENSURE_SUCCESS(rv, 1);

  {
    ReentrantMonitorAutoEnter mon(*gReentrantMonitor);

    for (PRUint32 i = 0; i < NUMBER_OF_THREADS; i++) {
      nsCOMPtr<nsIRunnable> runnable = new nsRunnable();
      NS_ENSURE_TRUE(runnable, 1);

      rv = pool->Dispatch(runnable, NS_DISPATCH_NORMAL);
      NS_ENSURE_SUCCESS(rv, 1);
    }

    gAllRunnablesPosted = PR_TRUE;
    mon.NotifyAll();
  }

  {
    ReentrantMonitorAutoEnter mon(*gReentrantMonitor);
    while (!gAllThreadsCreated) {
      mon.Wait();
    }
  }

  rv = pool->Shutdown();
  NS_ENSURE_SUCCESS(rv, 1);

  {
    ReentrantMonitorAutoEnter mon(*gReentrantMonitor);
    while (!gAllThreadsShutDown) {
      mon.Wait();
    }
  }

  for (PRUint32 i = 0; i < NUMBER_OF_THREADS; i++) {
    nsIThread* created = gCreatedThreadList[i];
    NS_ENSURE_TRUE(created, 1);

    bool match = false;
    for (PRUint32 j = 0; j < NUMBER_OF_THREADS; j++) {
      nsIThread* destroyed = gShutDownThreadList[j];
      NS_ENSURE_TRUE(destroyed, 1);

      if (destroyed == created) {
        match = PR_TRUE;
        break;
      }
    }

    NS_ENSURE_TRUE(match, 1);
  }

  return 0;
}
