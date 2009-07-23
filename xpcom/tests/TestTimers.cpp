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
 * The Original Code is Timer Test Code.
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

#include "nsIThread.h"
#include "nsITimer.h"

#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "prinrval.h"
#include "prmon.h"

#include "mozilla/Monitor.h"
using namespace mozilla;

typedef nsresult(*TestFuncPtr)();

class AutoTestThread
{
public:
  AutoTestThread() {
    nsCOMPtr<nsIThread> newThread;
    nsresult rv = NS_NewThread(getter_AddRefs(newThread));
    NS_ENSURE_SUCCESS(rv,);

    newThread.swap(mThread);
  }

  ~AutoTestThread() {
    mThread->Shutdown();
  }

  operator nsIThread*() const {
    return mThread;
  }

  nsIThread* operator->() const {
    return mThread;
  }

private:
  nsCOMPtr<nsIThread> mThread;
};

class AutoCreateAndDestroyMonitor
{
public:
  AutoCreateAndDestroyMonitor() {
    mMonitor = new Monitor("TestTimers::AutoMon");
    NS_ASSERTION(mMonitor, "Out of memory!");
  }

  ~AutoCreateAndDestroyMonitor() {
    if (mMonitor) {
      delete mMonitor;
    }
  }

  operator Monitor* () {
    return mMonitor;
  }

private:
  Monitor* mMonitor;
};

class TimerCallback : public nsITimerCallback
{
public:
  NS_DECL_ISUPPORTS

  TimerCallback(nsIThread** aThreadPtr, Monitor* aMonitor)
  : mThreadPtr(aThreadPtr), mMonitor(aMonitor) { }

  NS_IMETHOD Notify(nsITimer* aTimer) {
    nsCOMPtr<nsIThread> current(do_GetCurrentThread());

    MonitorAutoEnter mon(*mMonitor);

    NS_ASSERTION(!*mThreadPtr, "Timer called back more than once!");
    *mThreadPtr = current;

    mon.Notify();

    return NS_OK;
  }
private:
  nsIThread** mThreadPtr;
  Monitor* mMonitor;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(TimerCallback, nsITimerCallback)

nsresult
TestTargetedTimers()
{
  AutoCreateAndDestroyMonitor newMon;
  NS_ENSURE_TRUE(newMon, NS_ERROR_OUT_OF_MEMORY);

  AutoTestThread testThread;
  NS_ENSURE_TRUE(testThread, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv;
  nsCOMPtr<nsITimer> timer = do_CreateInstance(NS_TIMER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsIEventTarget* target = static_cast<nsIEventTarget*>(testThread);

  rv = timer->SetTarget(target);
  NS_ENSURE_SUCCESS(rv, rv);

  nsIThread* notifiedThread = nsnull;

  nsCOMPtr<nsITimerCallback> callback =
    new TimerCallback(&notifiedThread, newMon);
  NS_ENSURE_TRUE(callback, NS_ERROR_OUT_OF_MEMORY);

  rv = timer->InitWithCallback(callback, PR_MillisecondsToInterval(2000),
                               nsITimer::TYPE_ONE_SHOT);
  NS_ENSURE_SUCCESS(rv, rv);

  MonitorAutoEnter mon(*newMon);
  while (!notifiedThread) {
    mon.Wait();
  }
  NS_ENSURE_TRUE(notifiedThread == testThread, NS_ERROR_FAILURE);

  return NS_OK;
}

int main(int argc, char** argv)
{
  ScopedXPCOM xpcom("TestTimers");
  NS_ENSURE_FALSE(xpcom.failed(), 1);

  static TestFuncPtr testsToRun[] = {
    TestTargetedTimers
  };
  static PRUint32 testCount = sizeof(testsToRun) / sizeof(testsToRun[0]);

  for (PRUint32 i = 0; i < testCount; i++) {
    nsresult rv = testsToRun[i]();
    NS_ENSURE_SUCCESS(rv, 1);
  }

  return 0;
}
