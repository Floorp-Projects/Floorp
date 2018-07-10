/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "mozilla/AbstractThread.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/DocGroup.h"
#include "mozilla/dom/DOMPrefs.h"
#include "mozilla/dom/TabGroup.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/TaskCategory.h"
#include "mozilla/PerformanceCounter.h"
#include "nsThreadUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsThread.h"

using namespace mozilla;
using mozilla::Runnable;


class MockSchedulerGroup: public SchedulerGroup
{
public:
  explicit MockSchedulerGroup(mozilla::dom::DocGroup* aDocGroup)
      : mDocGroup(aDocGroup) {}
  NS_INLINE_DECL_REFCOUNTING(MockSchedulerGroup);

  MOCK_METHOD1(SetValidatingAccess, void(ValidationType aType));
  mozilla::dom::DocGroup* DocGroup() {
    return mDocGroup;
  }
protected:
  virtual ~MockSchedulerGroup() = default;
private:

  mozilla::dom::DocGroup* mDocGroup;
};


typedef testing::NiceMock<MockSchedulerGroup> MSchedulerGroup;

/* Timed runnable which simulates some execution time
 * and can run a nested runnable.
 */
class TimedRunnable final : public Runnable
{
public:
  explicit TimedRunnable(uint32_t aExecutionTime1, uint32_t aExecutionTime2,
                         uint32_t aSubExecutionTime)
    : Runnable("TimedRunnable")
    , mExecutionTime1(aExecutionTime1)
    , mExecutionTime2(aExecutionTime2)
    , mSubExecutionTime(aSubExecutionTime)
  {
  }
  NS_IMETHODIMP Run()
  {
    PR_Sleep(PR_MillisecondsToInterval(mExecutionTime1 + 5));
    if (mSubExecutionTime > 0) {
      // Dispatch another runnable so nsThread::ProcessNextEvent is called recursively
      nsCOMPtr<nsIThread> thread = do_GetMainThread();
      nsCOMPtr<nsIRunnable> runnable = new TimedRunnable(mSubExecutionTime, 0, 0);
      thread->Dispatch(runnable, NS_DISPATCH_NORMAL);
      (void)NS_ProcessNextEvent(thread, false);
    }
    PR_Sleep(PR_MillisecondsToInterval(mExecutionTime2 + 5));
    return NS_OK;
  }
private:
  uint32_t mExecutionTime1;
  uint32_t mExecutionTime2;
  uint32_t mSubExecutionTime;
};


/* test class used for all metrics tests
 *
 * - sets up the enable_scheduler_timing pref
 * - provides a function to dispatch runnables and spin the loop
 */

static const char prefKey[] = "dom.performance.enable_scheduler_timing";

class ThreadMetrics: public ::testing::Test
{
public:
  explicit ThreadMetrics() = default;

protected:
  virtual void SetUp() {
    mOldPref = Preferences::GetBool(prefKey);
    Preferences::SetBool(prefKey, true);
    // building the TabGroup/DocGroup structure
    nsCString key = NS_LITERAL_CSTRING("key");
    nsCOMPtr<nsIDocument> doc;
    RefPtr<mozilla::dom::TabGroup> tabGroup = new mozilla::dom::TabGroup(false);
    mDocGroup = tabGroup->AddDocument(key, doc);
    mSchedulerGroup = new MSchedulerGroup(mDocGroup);
    mCounter = mDocGroup->GetPerformanceCounter();
    mThreadMgr = do_GetService("@mozilla.org/thread-manager;1");
    mOther = DispatchCategory(TaskCategory::Other).GetValue();
    mDispatchCount = (uint32_t)TaskCategory::Other + 1;
  }

  virtual void TearDown() {
    // and remove the document from the doc group (actually, a nullptr)
    mDocGroup->RemoveDocument(nullptr);
    mDocGroup = nullptr;
    Preferences::SetBool(prefKey, mOldPref);
    ProcessAllEvents();
  }

  // this is used to get rid of transient events
  void initScheduler() {
    ProcessAllEvents();
  }

  nsresult Dispatch(uint32_t aExecutionTime1, uint32_t aExecutionTime2,
                    uint32_t aSubExecutionTime) {
    ProcessAllEvents();
    nsCOMPtr<nsIRunnable> runnable = new TimedRunnable(aExecutionTime1,
                                                       aExecutionTime2,
                                                       aSubExecutionTime);
    runnable = new SchedulerGroup::Runnable(runnable.forget(),
                                            mSchedulerGroup, mDocGroup);
    return mDocGroup->Dispatch(TaskCategory::Other, runnable.forget());
  }

  void ProcessAllEvents() {
    mThreadMgr->SpinEventLoopUntilEmpty();
  }

  uint32_t mOther;
  bool mOldPref;
  RefPtr<MSchedulerGroup> mSchedulerGroup;
  RefPtr<mozilla::dom::DocGroup> mDocGroup;
  RefPtr<mozilla::PerformanceCounter> mCounter;
  nsCOMPtr<nsIThreadManager> mThreadMgr;
  uint32_t mDispatchCount;
};


TEST_F(ThreadMetrics, CollectMetrics)
{
  nsresult rv;
  initScheduler();

  // Dispatching a runnable that will last for +50ms
  rv = Dispatch(25, 25, 0);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  // Flush the queue
  ProcessAllEvents();

  // Let's look at the task category "other" counter
  ASSERT_EQ(mCounter->GetDispatchCounter()[mOther], 1u);

  // other counters should stay empty
  for (uint32_t i = 0; i < mDispatchCount; i++) {
    if (i != mOther) {
        ASSERT_EQ(mCounter->GetDispatchCounter()[i], 0u);
    }
  }

  // Did we get incremented in the docgroup ?
  uint64_t duration = mCounter->GetExecutionDuration();
  ASSERT_GE(duration, 50000u);
  ASSERT_LT(duration, 200000u);
}


TEST_F(ThreadMetrics, CollectRecursiveMetrics)
{
  nsresult rv;

  initScheduler();

  // Dispatching a runnable that will last for +50ms
  // and run another one recursively that lasts for 200ms
  rv = Dispatch(25, 25, 200);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  // Flush the queue
  ProcessAllEvents();

  // let's look at the counters
  ASSERT_EQ(mCounter->GetDispatchCounter()[mOther], 1u);

  // other counters should stay empty
  for (uint32_t i = 0; i < mDispatchCount; i++) {
    if (i != mOther) {
        ASSERT_EQ(mCounter->GetDispatchCounter()[i], 0u);
    }
  }

  // did we get incremented in the docgroup ?
  uint64_t duration = mCounter->GetExecutionDuration();
  ASSERT_GE(duration, 50000u);

  // let's make sure we don't count the time spent in recursive calls
  ASSERT_LT(duration, 200000u);
}
