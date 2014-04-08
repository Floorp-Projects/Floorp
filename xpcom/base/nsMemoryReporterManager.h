/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMemoryReporterManager_h__
#define nsMemoryReporterManager_h__

#include "nsIMemoryReporter.h"
#include "nsITimer.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/Mutex.h"
#include "nsTHashtable.h"
#include "nsHashKeys.h"

class nsITimer;

namespace mozilla {
namespace dom {
class MemoryReport;
}
}

class nsMemoryReporterManager : public nsIMemoryReporterManager
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIMEMORYREPORTERMANAGER

  nsMemoryReporterManager();
  virtual ~nsMemoryReporterManager();

  // Gets the memory reporter manager service.
  static nsMemoryReporterManager* GetOrCreate()
  {
    nsCOMPtr<nsIMemoryReporterManager> imgr =
      do_GetService("@mozilla.org/memory-reporter-manager;1");
    return static_cast<nsMemoryReporterManager*>(imgr.get());
  }

  typedef nsTHashtable<nsRefPtrHashKey<nsIMemoryReporter> > StrongReportersTable;
  typedef nsTHashtable<nsPtrHashKey<nsIMemoryReporter> > WeakReportersTable;

  void IncrementNumChildProcesses();
  void DecrementNumChildProcesses();

  // Inter-process memory reporting proceeds as follows.
  //
  // - GetReports() (declared within NS_DECL_NSIMEMORYREPORTERMANAGER)
  //   synchronously gets memory reports for the current process, tells all
  //   child processes to get memory reports, and sets up some state
  //   (mGetReportsState) for when the child processes report back, including a
  //   timer.  Control then returns to the main event loop.
  //
  // - HandleChildReports() is called (asynchronously) once per child process
  //   that reports back.  If all child processes report back before time-out,
  //   the timer is cancelled.  (The number of child processes is part of the
  //   saved request state.)
  //
  // - TimeoutCallback() is called (asynchronously) if all the child processes
  //   don't respond within the time threshold.
  //
  // - FinishReporting() finishes things off.  It is *always* called -- either
  //   from HandleChildReports() (if all child processes have reported back) or
  //   from TimeoutCallback() (if time-out occurs).
  //
  // All operations occur on the main thread.
  //
  // The above sequence of steps is a "request".  A partially-completed request
  // is described as "in flight".
  //
  // Each request has a "generation", a unique number that identifies it.  This
  // is used to ensure that each reports from a child process corresponds to
  // the appropriate request from the parent process.  (It's easier to
  // implement a generation system than to implement a child report request
  // cancellation mechanism.)
  //
  // Failures are mostly ignored, because it's (a) typically the most sensible
  // thing to do, and (b) often hard to do anything else.  The following are
  // the failure cases of note.
  //
  // - If a request is made while the previous request is in flight, the new
  //   request is ignored, as per getReports()'s specification.  No error is
  //   reported, because the previous request will complete soon enough.
  //
  // - If one or more child processes fail to respond within the time limit,
  //   things will proceed as if they don't exist.  No error is reported,
  //   because partial information is better than nothing.
  //
  // - If a child process reports after the time-out occurs, it is ignored.
  //   (Generation checking will ensure it is ignored even if a subsequent
  //   request is in flight;  this is the main use of generations.)  No error
  //   is reported, because there's nothing sensible to be done about it at
  //   this late stage.
  //
  // Now, what what happens if a child process is created/destroyed in the
  // middle of a request?  Well, GetReportsState contains a copy of
  // mNumChildProcesses which it uses to determine finished-ness.  So...
  //
  // - If a process is created, it won't have received the request for reports,
  //   and the GetReportsState's mNumChildProcesses won't account for it.  So
  //   the reported data will reflect how things were when the request began.
  //
  // - If a process is destroyed before reporting back, we'll just hit the
  //   time-out, because we'll have received reports (barring other errors)
  //   from N-1 child process.  So the reported data will reflect how things
  //   are when the request ends.
  //
  // - If a process is destroyed after reporting back, but before all other
  //   child processes have reported back, it will be included in the reported
  //   data.  So the reported data will reflect how things were when the
  //   request began.
  //
  // The inconsistencies between these three cases are unfortunate but
  // difficult to avoid.  It's enough of an edge case to not be worth doing
  // more.
  //
  void HandleChildReports(
    const uint32_t& generation,
    const InfallibleTArray<mozilla::dom::MemoryReport>& aChildReports);
  nsresult FinishReporting();

  // Functions that (a) implement distinguished amounts, and (b) are outside of
  // this module.
  struct AmountFns
  {
    mozilla::InfallibleAmountFn mJSMainRuntimeGCHeap;
    mozilla::InfallibleAmountFn mJSMainRuntimeTemporaryPeak;
    mozilla::InfallibleAmountFn mJSMainRuntimeCompartmentsSystem;
    mozilla::InfallibleAmountFn mJSMainRuntimeCompartmentsUser;

    mozilla::InfallibleAmountFn mImagesContentUsedUncompressed;

    mozilla::InfallibleAmountFn mStorageSQLite;

    mozilla::InfallibleAmountFn mLowMemoryEventsVirtual;
    mozilla::InfallibleAmountFn mLowMemoryEventsPhysical;

    mozilla::InfallibleAmountFn mGhostWindows;

    AmountFns() { mozilla::PodZero(this); }
  };
  AmountFns mAmountFns;

  // Convenience function to get RSS easily from other code.  This is useful
  // when debugging transient memory spikes with printf instrumentation.
  static int64_t ResidentFast();

  // Functions that measure per-tab memory consumption.
  struct SizeOfTabFns
  {
    mozilla::JSSizeOfTabFn    mJS;
    mozilla::NonJSSizeOfTabFn mNonJS;

    SizeOfTabFns()
    {
      mozilla::PodZero(this);
    }
  };
  SizeOfTabFns mSizeOfTabFns;

private:
  nsresult RegisterReporterHelper(nsIMemoryReporter* aReporter,
                                  bool aForce, bool aStrongRef);
  nsresult StartGettingReports();

  static void TimeoutCallback(nsITimer* aTimer, void* aData);
  // Note: this timeout needs to be long enough to allow for the
  // possibility of DMD reports and/or running on a low-end phone.
  static const uint32_t kTimeoutLengthMS = 50000;

  mozilla::Mutex mMutex;
  bool mIsRegistrationBlocked;

  StrongReportersTable* mStrongReporters;
  WeakReportersTable* mWeakReporters;

  // These two are only used for testing purposes.
  StrongReportersTable* mSavedStrongReporters;
  WeakReportersTable* mSavedWeakReporters;

  uint32_t mNumChildProcesses;
  uint32_t mNextGeneration;

  struct GetReportsState
  {
    uint32_t                             mGeneration;
    nsCOMPtr<nsITimer>                   mTimer;
    uint32_t                             mNumChildProcesses;
    uint32_t                             mNumChildProcessesCompleted;
    nsCOMPtr<nsIHandleReportCallback>    mHandleReport;
    nsCOMPtr<nsISupports>                mHandleReportData;
    nsCOMPtr<nsIFinishReportingCallback> mFinishReporting;
    nsCOMPtr<nsISupports>                mFinishReportingData;
    nsString                             mDMDDumpIdent;

    GetReportsState(uint32_t aGeneration, nsITimer* aTimer,
                    uint32_t aNumChildProcesses,
                    nsIHandleReportCallback* aHandleReport,
                    nsISupports* aHandleReportData,
                    nsIFinishReportingCallback* aFinishReporting,
                    nsISupports* aFinishReportingData,
                    const nsAString &aDMDDumpIdent)
      : mGeneration(aGeneration)
      , mTimer(aTimer)
      , mNumChildProcesses(aNumChildProcesses)
      , mNumChildProcessesCompleted(0)
      , mHandleReport(aHandleReport)
      , mHandleReportData(aHandleReportData)
      , mFinishReporting(aFinishReporting)
      , mFinishReportingData(aFinishReportingData)
      , mDMDDumpIdent(aDMDDumpIdent)
    {
    }
  };

  // When this is non-null, a request is in flight.  Note: We use manual
  // new/delete for this because its lifetime doesn't match block scope or
  // anything like that.
  GetReportsState* mGetReportsState;
};

#define NS_MEMORY_REPORTER_MANAGER_CID \
{ 0xfb97e4f5, 0x32dd, 0x497a, \
{ 0xba, 0xa2, 0x7d, 0x1e, 0x55, 0x7, 0x99, 0x10 } }

#endif // nsMemoryReporterManager_h__
