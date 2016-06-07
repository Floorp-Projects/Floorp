/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- *//* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPerformanceStats.h"

#include "nsMemory.h"
#include "nsLiteralString.h"
#include "nsCRTGlue.h"
#include "nsServiceManagerUtils.h"

#include "nsCOMArray.h"
#include "nsContentUtils.h"
#include "nsIMutableArray.h"
#include "nsReadableUtils.h"

#include "jsapi.h"
#include "nsJSUtils.h"
#include "xpcpublic.h"
#include "jspubtd.h"

#include "nsIDOMWindow.h"
#include "nsGlobalWindow.h"
#include "nsRefreshDriver.h"

#include "mozilla/unused.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/Services.h"
#include "mozilla/Telemetry.h"

#if defined(XP_WIN)
#include <processthreadsapi.h>
#include <windows.h>
#else
#include <unistd.h>
#endif // defined(XP_WIN)

#if defined(XP_MACOSX)
#include <mach/mach_init.h>
#include <mach/mach_interface.h>
#include <mach/mach_port.h>
#include <mach/mach_types.h>
#include <mach/message.h>
#include <mach/thread_info.h>
#elif defined(XP_UNIX)
#include <sys/time.h>
#include <sys/resource.h>
#endif // defined(XP_UNIX)
/* ------------------------------------------------------
 *
 * Utility functions.
 *
 */

namespace {

/**
 * Get the private window for the current compartment.
 *
 * @return null if the code is not executed in a window or in
 * case of error, a nsPIDOMWindow otherwise.
 */
already_AddRefed<nsPIDOMWindowOuter>
GetPrivateWindow(JSContext* cx) {
  nsGlobalWindow* win = xpc::CurrentWindowOrNull(cx);
  if (!win) {
    return nullptr;
  }

  nsPIDOMWindowOuter* outer = win->AsInner()->GetOuterWindow();
  if (!outer) {
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindowOuter> top = outer->GetTop();
  if (!top) {
    return nullptr;
  }

  return top.forget();
}

bool
URLForGlobal(JSContext* cx, JS::Handle<JSObject*> global, nsAString& url) {
  nsCOMPtr<nsIPrincipal> principal = nsContentUtils::ObjectPrincipal(global);
  if (!principal) {
    return false;
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = principal->GetURI(getter_AddRefs(uri));
  if (NS_FAILED(rv) || !uri) {
    return false;
  }

  nsAutoCString spec;
  rv = uri->GetSpec(spec);
  if (NS_FAILED(rv)) {
    return false;
  }

  url.Assign(NS_ConvertUTF8toUTF16(spec));
  return true;
}

/**
 * Extract a somewhat human-readable name from the current context.
 */
void
CompartmentName(JSContext* cx, JS::Handle<JSObject*> global, nsAString& name) {
  // Attempt to use the URL as name.
  if (URLForGlobal(cx, global, name)) {
    return;
  }

  // Otherwise, fallback to XPConnect's less readable but more
  // complete naming scheme.
  nsAutoCString cname;
  xpc::GetCurrentCompartmentName(cx, cname);
  name.Assign(NS_ConvertUTF8toUTF16(cname));
}

/**
 * Generate a unique-to-the-application identifier for a group.
 */
void
GenerateUniqueGroupId(const JSRuntime* rt, uint64_t uid, uint64_t processId, nsAString& groupId) {
  uint64_t runtimeId = reinterpret_cast<uintptr_t>(rt);

  groupId.AssignLiteral("process: ");
  groupId.AppendInt(processId);
  groupId.AppendLiteral(", thread: ");
  groupId.AppendInt(runtimeId);
  groupId.AppendLiteral(", group: ");
  groupId.AppendInt(uid);
}

static const char* TOPICS[] = {
  "profile-before-change",
  "quit-application",
  "quit-application-granted",
  "xpcom-will-shutdown"
};

} // namespace

/* ------------------------------------------------------
 *
 * class nsPerformanceObservationTarget
 *
 */


NS_IMPL_ISUPPORTS(nsPerformanceObservationTarget, nsIPerformanceObservable)



NS_IMETHODIMP
nsPerformanceObservationTarget::GetTarget(nsIPerformanceGroupDetails** _result) {
  if (mDetails) {
    NS_IF_ADDREF(*_result = mDetails);
  }
  return NS_OK;
};

void
nsPerformanceObservationTarget::SetTarget(nsPerformanceGroupDetails* details) {
  MOZ_ASSERT(!mDetails);
  mDetails = details;
};

NS_IMETHODIMP
nsPerformanceObservationTarget::AddJankObserver(nsIPerformanceObserver* observer) {
  if (!mObservers.append(observer)) {
    MOZ_CRASH();
  }
  return NS_OK;
};

NS_IMETHODIMP
nsPerformanceObservationTarget::RemoveJankObserver(nsIPerformanceObserver* observer) {
  for (auto iter = mObservers.begin(), end = mObservers.end(); iter < end; ++iter) {
    if (*iter == observer) {
      mObservers.erase(iter);
      return NS_OK;
    }
  }
  return NS_OK;
};

bool
nsPerformanceObservationTarget::HasObservers() const {
  return !mObservers.empty();
}

void
nsPerformanceObservationTarget::NotifyJankObservers(nsIPerformanceGroupDetails* source, nsIPerformanceAlert* gravity) {
  // Copy the vector to make sure that it won't change under our feet.
  mozilla::Vector<nsCOMPtr<nsIPerformanceObserver>> observers;
  if (!observers.appendAll(mObservers)) {
    MOZ_CRASH();
  }

  // Now actually notify.
  for (auto iter = observers.begin(), end = observers.end(); iter < end; ++iter) {
    nsCOMPtr<nsIPerformanceObserver> observer = *iter;
    mozilla::Unused << observer->Observe(source, gravity);
  }
}

/* ------------------------------------------------------
 *
 * class nsGroupHolder
 *
 */

nsPerformanceObservationTarget*
nsGroupHolder::ObservationTarget() {
  if (!mPendingObservationTarget) {
    mPendingObservationTarget = new nsPerformanceObservationTarget();
  }
  return mPendingObservationTarget;
}

nsPerformanceGroup*
nsGroupHolder::GetGroup() {
  return mGroup;
}

void
nsGroupHolder::SetGroup(nsPerformanceGroup* group) {
  MOZ_ASSERT(!mGroup);
  mGroup = group;
  group->SetObservationTarget(ObservationTarget());
  mPendingObservationTarget->SetTarget(group->Details());
}

/* ------------------------------------------------------
 *
 * struct PerformanceData
 *
 */

PerformanceData::PerformanceData()
  : mTotalUserTime(0)
  , mTotalSystemTime(0)
  , mTotalCPOWTime(0)
  , mTicks(0)
{
  mozilla::PodArrayZero(mDurations);
}

/* ------------------------------------------------------
 *
 * class nsPerformanceGroupDetails
 *
 */

NS_IMPL_ISUPPORTS(nsPerformanceGroupDetails, nsIPerformanceGroupDetails)

const nsAString&
nsPerformanceGroupDetails::Name() const {
  return mName;
}

const nsAString&
nsPerformanceGroupDetails::GroupId() const {
  return mGroupId;
}

const nsAString&
nsPerformanceGroupDetails::AddonId() const {
  return mAddonId;
}

uint64_t
nsPerformanceGroupDetails::WindowId() const {
  return mWindowId;
}

uint64_t
nsPerformanceGroupDetails::ProcessId() const {
  return mProcessId;
}

bool
nsPerformanceGroupDetails::IsSystem() const {
  return mIsSystem;
}

bool
nsPerformanceGroupDetails::IsAddon() const {
  return mAddonId.Length() != 0;
}

bool
nsPerformanceGroupDetails::IsWindow() const {
  return mWindowId != 0;
}

bool
nsPerformanceGroupDetails::IsContentProcess() const {
  return XRE_GetProcessType() == GeckoProcessType_Content;
}

/* readonly attribute AString name; */
NS_IMETHODIMP
nsPerformanceGroupDetails::GetName(nsAString& aName) {
  aName.Assign(Name());
  return NS_OK;
};

/* readonly attribute AString groupId; */
NS_IMETHODIMP
nsPerformanceGroupDetails::GetGroupId(nsAString& aGroupId) {
  aGroupId.Assign(GroupId());
  return NS_OK;
};

/* readonly attribute AString addonId; */
NS_IMETHODIMP
nsPerformanceGroupDetails::GetAddonId(nsAString& aAddonId) {
  aAddonId.Assign(AddonId());
  return NS_OK;
};

/* readonly attribute uint64_t windowId; */
NS_IMETHODIMP
nsPerformanceGroupDetails::GetWindowId(uint64_t *aWindowId) {
  *aWindowId = WindowId();
  return NS_OK;
}

/* readonly attribute bool isSystem; */
NS_IMETHODIMP
nsPerformanceGroupDetails::GetIsSystem(bool *_retval) {
  *_retval = IsSystem();
  return NS_OK;
}

/*
  readonly attribute unsigned long long processId;
*/
NS_IMETHODIMP
nsPerformanceGroupDetails::GetProcessId(uint64_t* processId) {
  *processId = ProcessId();
  return NS_OK;
}

/* readonly attribute bool IsContentProcess; */
NS_IMETHODIMP
nsPerformanceGroupDetails::GetIsContentProcess(bool *_retval) {
  *_retval = IsContentProcess();
  return NS_OK;
}


/* ------------------------------------------------------
 *
 * class nsPerformanceStats
 *
 */

class nsPerformanceStats final: public nsIPerformanceStats
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPERFORMANCESTATS
  NS_FORWARD_NSIPERFORMANCEGROUPDETAILS(mDetails->)

  nsPerformanceStats(nsPerformanceGroupDetails* item,
                     const PerformanceData& aPerformanceData)
    : mDetails(item)
    , mPerformanceData(aPerformanceData)
  {
  }


private:
  RefPtr<nsPerformanceGroupDetails> mDetails;
  PerformanceData mPerformanceData;

  ~nsPerformanceStats() {}
};

NS_IMPL_ISUPPORTS(nsPerformanceStats, nsIPerformanceStats, nsIPerformanceGroupDetails)

/* readonly attribute unsigned long long totalUserTime; */
NS_IMETHODIMP
nsPerformanceStats::GetTotalUserTime(uint64_t *aTotalUserTime) {
  *aTotalUserTime = mPerformanceData.mTotalUserTime;
  return NS_OK;
};

/* readonly attribute unsigned long long totalSystemTime; */
NS_IMETHODIMP
nsPerformanceStats::GetTotalSystemTime(uint64_t *aTotalSystemTime) {
  *aTotalSystemTime = mPerformanceData.mTotalSystemTime;
  return NS_OK;
};

/* readonly attribute unsigned long long totalCPOWTime; */
NS_IMETHODIMP
nsPerformanceStats::GetTotalCPOWTime(uint64_t *aCpowTime) {
  *aCpowTime = mPerformanceData.mTotalCPOWTime;
  return NS_OK;
};

/* readonly attribute unsigned long long ticks; */
NS_IMETHODIMP
nsPerformanceStats::GetTicks(uint64_t *aTicks) {
  *aTicks = mPerformanceData.mTicks;
  return NS_OK;
};

/* void getDurations (out unsigned long aCount, [array, size_is (aCount), retval] out unsigned long long aNumberOfOccurrences); */
NS_IMETHODIMP
nsPerformanceStats::GetDurations(uint32_t *aCount, uint64_t **aNumberOfOccurrences) {
  const size_t length = mozilla::ArrayLength(mPerformanceData.mDurations);
  if (aCount) {
    *aCount = length;
  }
  *aNumberOfOccurrences = new uint64_t[length];
  for (size_t i = 0; i < length; ++i) {
    (*aNumberOfOccurrences)[i] = mPerformanceData.mDurations[i];
  }
  return NS_OK;
};


/* ------------------------------------------------------
 *
 * struct nsPerformanceSnapshot
 *
 */

class nsPerformanceSnapshot final : public nsIPerformanceSnapshot
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPERFORMANCESNAPSHOT

  nsPerformanceSnapshot() {}

  /**
   * Append statistics to the list of components data.
   */
  void AppendComponentsStats(nsIPerformanceStats* stats);

  /**
   * Set the statistics attached to process data.
   */
  void SetProcessStats(nsIPerformanceStats* group);

private:
  ~nsPerformanceSnapshot() {}

private:
  /**
   * The data for all components.
   */
  nsCOMArray<nsIPerformanceStats> mComponentsData;

  /**
   * The data for the process.
   */
  nsCOMPtr<nsIPerformanceStats> mProcessData;
};

NS_IMPL_ISUPPORTS(nsPerformanceSnapshot, nsIPerformanceSnapshot)


/* nsIArray getComponentsData (); */
NS_IMETHODIMP
nsPerformanceSnapshot::GetComponentsData(nsIArray * *aComponents)
{
  const size_t length = mComponentsData.Length();
  nsCOMPtr<nsIMutableArray> components = do_CreateInstance(NS_ARRAY_CONTRACTID);
  for (size_t i = 0; i < length; ++i) {
    nsCOMPtr<nsIPerformanceStats> stats = mComponentsData[i];
    mozilla::DebugOnly<nsresult> rv = components->AppendElement(stats, false);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }
  components.forget(aComponents);
  return NS_OK;
}

/* nsIPerformanceStats getProcessData (); */
NS_IMETHODIMP
nsPerformanceSnapshot::GetProcessData(nsIPerformanceStats * *aProcess)
{
  NS_IF_ADDREF(*aProcess = mProcessData);
  return NS_OK;
}

void
nsPerformanceSnapshot::AppendComponentsStats(nsIPerformanceStats* stats)
{
  mComponentsData.AppendElement(stats);
}

void
nsPerformanceSnapshot::SetProcessStats(nsIPerformanceStats* stats)
{
  mProcessData = stats;
}



/* ------------------------------------------------------
 *
 * class PerformanceAlert
 *
 */
class PerformanceAlert final: public nsIPerformanceAlert {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPERFORMANCEALERT

  PerformanceAlert(const uint32_t reason, nsPerformanceGroup* source);
private:
  ~PerformanceAlert() {}

  const uint32_t mReason;

  // The highest values reached by this group since the latest alert,
  // in microseconds.
  const uint64_t mHighestJank;
  const uint64_t mHighestCPOW;
};

NS_IMPL_ISUPPORTS(PerformanceAlert, nsIPerformanceAlert);

PerformanceAlert::PerformanceAlert(const uint32_t reason, nsPerformanceGroup* source)
  : mReason(reason)
  , mHighestJank(source->HighestRecentJank())
  , mHighestCPOW(source->HighestRecentCPOW())
{ }

NS_IMETHODIMP
PerformanceAlert::GetHighestJank(uint64_t* result) {
  *result = mHighestJank;
  return NS_OK;
}

NS_IMETHODIMP
PerformanceAlert::GetHighestCPOW(uint64_t* result) {
  *result = mHighestCPOW;
  return NS_OK;
}

NS_IMETHODIMP
PerformanceAlert::GetReason(uint32_t* result) {
  *result = mReason;
  return NS_OK;
}
/* ------------------------------------------------------
 *
 * class PendingAlertsCollector
 *
 */

/**
 * A timer callback in charge of collecting the groups in
 * `mPendingAlerts` and triggering dispatch of performance alerts.
 */
class PendingAlertsCollector final: public nsITimerCallback {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK

  PendingAlertsCollector(JSRuntime* runtime, nsPerformanceStatsService* service)
    : mService(service)
    , mPending(false)
  { }

  nsresult Start(uint32_t timerDelayMS);
  nsresult Dispose();

private:
  ~PendingAlertsCollector() {}

  RefPtr<nsPerformanceStatsService> mService;
  bool mPending;

  nsCOMPtr<nsITimer> mTimer;

  mozilla::Vector<uint64_t> mJankLevels;
};

NS_IMPL_ISUPPORTS(PendingAlertsCollector, nsITimerCallback);

NS_IMETHODIMP
PendingAlertsCollector::Notify(nsITimer*) {
  mPending = false;
  mService->NotifyJankObservers(mJankLevels);
  return NS_OK;
}

nsresult
PendingAlertsCollector::Start(uint32_t timerDelayMS) {
  if (mPending) {
    // Collector is already started.
    return NS_OK;
  }

  if (!mTimer) {
    mTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
  }

  nsresult rv = mTimer->InitWithCallback(this, timerDelayMS, nsITimer::TYPE_ONE_SHOT);
  if (NS_FAILED(rv)) {
    return rv;
  }

  mPending = true;
  {
    mozilla::DebugOnly<bool> result = nsRefreshDriver::GetJankLevels(mJankLevels);
    MOZ_ASSERT(result);
  }

  return NS_OK;
}

nsresult
PendingAlertsCollector::Dispose() {
  if (mTimer) {
    mozilla::Unused << mTimer->Cancel();
    mTimer = nullptr;
  }
  mService = nullptr;
  return NS_OK;
}



/* ------------------------------------------------------
 *
 * class nsPerformanceStatsService
 *
 */

NS_IMPL_ISUPPORTS(nsPerformanceStatsService, nsIPerformanceStatsService, nsIObserver)

nsPerformanceStatsService::nsPerformanceStatsService()
  : mIsAvailable(false)
  , mDisposed(false)
#if defined(XP_WIN)
  , mProcessId(GetCurrentProcessId())
#else
  , mProcessId(getpid())
#endif
  , mRuntime(xpc::GetJSRuntime())
  , mUIdCounter(0)
  , mTopGroup(nsPerformanceGroup::Make(mRuntime,
                                       this,
                                       NS_LITERAL_STRING("<process>"), // name
                                       NS_LITERAL_STRING(""),          // addonid
                                       0,    // windowId
                                       mProcessId,
                                       true, // isSystem
                                       nsPerformanceGroup::GroupScope::RUNTIME // scope
                                     ))
  , mIsHandlingUserInput(false)
  , mProcessStayed(0)
  , mProcessMoved(0)
  , mProcessUpdateCounter(0)
  , mIsMonitoringPerCompartment(false)
  , mJankAlertThreshold(mozilla::MaxValue<uint64_t>::value) // By default, no alerts
  , mJankAlertBufferingDelay(1000 /* ms */)
  , mJankLevelVisibilityThreshold(/* 2 ^ */ 8 /* ms */)
  , mMaxExpectedDurationOfInteractionUS(150 * 1000)
{
  mPendingAlertsCollector = new PendingAlertsCollector(mRuntime, this);

  // Attach some artificial group information to the universal listeners, to aid with debugging.
  nsString groupIdForAddons;
  GenerateUniqueGroupId(mRuntime, GetNextId(), mProcessId, groupIdForAddons);
  mUniversalTargets.mAddons->
    SetTarget(new nsPerformanceGroupDetails(NS_LITERAL_STRING("<universal add-on listener>"),
                                            groupIdForAddons,
                                            NS_LITERAL_STRING("<universal add-on listener>"),
                                            0, // window id
                                            mProcessId,
                                            false));


  nsString groupIdForWindows;
  GenerateUniqueGroupId(mRuntime, GetNextId(), mProcessId, groupIdForWindows);
  mUniversalTargets.mWindows->
    SetTarget(new nsPerformanceGroupDetails(NS_LITERAL_STRING("<universal window listener>"),
                                            groupIdForWindows,
                                            NS_LITERAL_STRING("<universal window listener>"),
                                            0, // window id
                                            mProcessId,
                                            false));
}

nsPerformanceStatsService::~nsPerformanceStatsService()
{ }

/**
 * Clean up the service.
 *
 * Called during shutdown. Idempotent.
 */
void
nsPerformanceStatsService::Dispose()
{
  // Make sure that we do not accidentally destroy `this` while we are
  // cleaning up back references.
  RefPtr<nsPerformanceStatsService> kungFuDeathGrip(this);
  mIsAvailable = false;

  if (mDisposed) {
    // Make sure that we don't double-dispose.
    return;
  }
  mDisposed = true;

  // Disconnect from nsIObserverService.
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    for (size_t i = 0; i < mozilla::ArrayLength(TOPICS); ++i) {
      mozilla::Unused << obs->RemoveObserver(this, TOPICS[i]);
    }
  }

  // Clear up and disconnect from JSAPI.
  js::DisposePerformanceMonitoring(mRuntime);

  mozilla::Unused << js::SetStopwatchIsMonitoringCPOW(mRuntime, false);
  mozilla::Unused << js::SetStopwatchIsMonitoringJank(mRuntime, false);

  mozilla::Unused << js::SetStopwatchStartCallback(mRuntime, nullptr, nullptr);
  mozilla::Unused << js::SetStopwatchCommitCallback(mRuntime, nullptr, nullptr);
  mozilla::Unused << js::SetGetPerformanceGroupsCallback(mRuntime, nullptr, nullptr);

  // Clear up and disconnect the alerts collector.
  if (mPendingAlertsCollector) {
    mPendingAlertsCollector->Dispose();
    mPendingAlertsCollector = nullptr;
  }
  mPendingAlerts.clear();

  // Disconnect universal observers. Per-group observers will be
  // disconnected below as part of `group->Dispose()`.
  mUniversalTargets.mAddons = nullptr;
  mUniversalTargets.mWindows = nullptr;

  // At this stage, the JS VM may still be holding references to
  // instances of PerformanceGroup on the stack. To let the service be
  // collected, we need to break the references from these groups to
  // `this`.
  mTopGroup->Dispose();
  mTopGroup = nullptr;

  // Copy references to the groups to a vector to ensure that we do
  // not modify the hashtable while iterating it.
  GroupVector groups;
  for (auto iter = mGroups.Iter(); !iter.Done(); iter.Next()) {
    if (!groups.append(iter.Get()->GetKey())) {
      MOZ_CRASH();
    }
  }
  for (auto iter = groups.begin(), end = groups.end(); iter < end; ++iter) {
    RefPtr<nsPerformanceGroup> group = *iter;
    group->Dispose();
  }

  // Any remaining references to PerformanceGroup will be released as
  // the VM unrolls the stack. If there are any nested event loops,
  // this may take time.
}

nsresult
nsPerformanceStatsService::Init()
{
  nsresult rv = InitInternal();
  if (NS_FAILED(rv)) {
    // Attempt to clean up.
    Dispose();
  }
  return rv;
}

nsresult
nsPerformanceStatsService::InitInternal()
{
  // Make sure that we release everything during shutdown.
  // We are a bit defensive here, as we know that some strange behavior can break the
  // regular shutdown order.
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    for (size_t i = 0; i < mozilla::ArrayLength(TOPICS); ++i) {
      mozilla::Unused << obs->AddObserver(this, TOPICS[i], false);
    }
  }

  // Connect to JSAPI.
  if (!js::SetStopwatchStartCallback(mRuntime, StopwatchStartCallback, this)) {
    return NS_ERROR_UNEXPECTED;
  }
  if (!js::SetStopwatchCommitCallback(mRuntime, StopwatchCommitCallback, this)) {
    return NS_ERROR_UNEXPECTED;
  }
  if (!js::SetGetPerformanceGroupsCallback(mRuntime, GetPerformanceGroupsCallback, this)) {
    return NS_ERROR_UNEXPECTED;
  }

  mTopGroup->setIsActive(true);
  mIsAvailable = true;

  return NS_OK;
}

// Observe shutdown events.
NS_IMETHODIMP
nsPerformanceStatsService::Observe(nsISupports *aSubject, const char *aTopic,
                                   const char16_t *aData)
{
  MOZ_ASSERT(strcmp(aTopic, "profile-before-change") == 0
             || strcmp(aTopic, "quit-application") == 0
             || strcmp(aTopic, "quit-application-granted") == 0
             || strcmp(aTopic, "xpcom-will-shutdown") == 0);

  Dispose();
  return NS_OK;
}

/*static*/ bool
nsPerformanceStatsService::IsHandlingUserInput() {
  if (mozilla::EventStateManager::LatestUserInputStart().IsNull()) {
    return false;
  }
  bool result = mozilla::TimeStamp::Now() - mozilla::EventStateManager::LatestUserInputStart() <= mozilla::TimeDuration::FromMicroseconds(mMaxExpectedDurationOfInteractionUS);
  return result;
}

/* [implicit_jscontext] attribute bool isMonitoringCPOW; */
NS_IMETHODIMP
nsPerformanceStatsService::GetIsMonitoringCPOW(JSContext* cx, bool *aIsStopwatchActive)
{
  if (!mIsAvailable) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  JSRuntime *runtime = JS_GetRuntime(cx);
  *aIsStopwatchActive = js::GetStopwatchIsMonitoringCPOW(runtime);
  return NS_OK;
}
NS_IMETHODIMP
nsPerformanceStatsService::SetIsMonitoringCPOW(JSContext* cx, bool aIsStopwatchActive)
{
  if (!mIsAvailable) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  JSRuntime *runtime = JS_GetRuntime(cx);
  if (!js::SetStopwatchIsMonitoringCPOW(runtime, aIsStopwatchActive)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

/* [implicit_jscontext] attribute bool isMonitoringJank; */
NS_IMETHODIMP
nsPerformanceStatsService::GetIsMonitoringJank(JSContext* cx, bool *aIsStopwatchActive)
{
  if (!mIsAvailable) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  JSRuntime *runtime = JS_GetRuntime(cx);
  *aIsStopwatchActive = js::GetStopwatchIsMonitoringJank(runtime);
  return NS_OK;
}
NS_IMETHODIMP
nsPerformanceStatsService::SetIsMonitoringJank(JSContext* cx, bool aIsStopwatchActive)
{
  if (!mIsAvailable) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  JSRuntime *runtime = JS_GetRuntime(cx);
  if (!js::SetStopwatchIsMonitoringJank(runtime, aIsStopwatchActive)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

/* [implicit_jscontext] attribute bool isMonitoringPerCompartment; */
NS_IMETHODIMP
nsPerformanceStatsService::GetIsMonitoringPerCompartment(JSContext*, bool *aIsMonitoringPerCompartment)
{
  if (!mIsAvailable) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  *aIsMonitoringPerCompartment = mIsMonitoringPerCompartment;
  return NS_OK;
}
NS_IMETHODIMP
nsPerformanceStatsService::SetIsMonitoringPerCompartment(JSContext*, bool aIsMonitoringPerCompartment)
{
  if (!mIsAvailable) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (aIsMonitoringPerCompartment == mIsMonitoringPerCompartment) {
    return NS_OK;
  }

  // Relatively slow update: walk the entire lost of performance groups,
  // update the active flag of those that have changed.
  //
  // Alternative strategies could be envisioned to make the update
  // much faster, at the expense of the speed of calling `isActive()`,
  // (e.g. deferring `isActive()` to the nsPerformanceStatsService),
  // but we expect that `isActive()` can be called thousands of times
  // per second, while `SetIsMonitoringPerCompartment` is not called
  // at all during most Firefox runs.

  for (auto iter = mGroups.Iter(); !iter.Done(); iter.Next()) {
    RefPtr<nsPerformanceGroup> group = iter.Get()->GetKey();
    if (group->Scope() == nsPerformanceGroup::GroupScope::COMPARTMENT) {
      group->setIsActive(aIsMonitoringPerCompartment);
    }
  }
  mIsMonitoringPerCompartment = aIsMonitoringPerCompartment;
  return NS_OK;
}

NS_IMETHODIMP
nsPerformanceStatsService::GetJankAlertThreshold(uint64_t* result) {
  *result = mJankAlertThreshold;
  return NS_OK;
}

NS_IMETHODIMP
nsPerformanceStatsService::SetJankAlertThreshold(uint64_t value) {
  mJankAlertThreshold = value;
  return NS_OK;
}

NS_IMETHODIMP
nsPerformanceStatsService::GetJankAlertBufferingDelay(uint32_t* result) {
  *result = mJankAlertBufferingDelay;
  return NS_OK;
}

NS_IMETHODIMP
nsPerformanceStatsService::SetJankAlertBufferingDelay(uint32_t value) {
  mJankAlertBufferingDelay = value;
  return NS_OK;
}

nsresult
nsPerformanceStatsService::UpdateTelemetry()
{
  // Promote everything to floating-point explicitly before dividing.
  const double processStayed = mProcessStayed;
  const double processMoved = mProcessMoved;

  if (processStayed <= 0 || processMoved <= 0 || processStayed + processMoved <= 0) {
    // Overflow/underflow/nothing to report
    return NS_OK;
  }

  const double proportion = (100 * processStayed) / (processStayed + processMoved);
  if (proportion < 0 || proportion > 100) {
    // Overflow/underflow
    return NS_OK;
  }

  mozilla::Telemetry::Accumulate(mozilla::Telemetry::PERF_MONITORING_TEST_CPU_RESCHEDULING_PROPORTION_MOVED, (uint32_t)proportion);
  return NS_OK;
}


/* static */ nsIPerformanceStats*
nsPerformanceStatsService::GetStatsForGroup(const js::PerformanceGroup* group)
{
  return GetStatsForGroup(nsPerformanceGroup::Get(group));
}

/* static */ nsIPerformanceStats*
nsPerformanceStatsService::GetStatsForGroup(const nsPerformanceGroup* group)
{
  return new nsPerformanceStats(group->Details(), group->data);
}

/* [implicit_jscontext] nsIPerformanceSnapshot getSnapshot (); */
NS_IMETHODIMP
nsPerformanceStatsService::GetSnapshot(JSContext* cx, nsIPerformanceSnapshot * *aSnapshot)
{
  if (!mIsAvailable) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  RefPtr<nsPerformanceSnapshot> snapshot = new nsPerformanceSnapshot();
  snapshot->SetProcessStats(GetStatsForGroup(mTopGroup));

  for (auto iter = mGroups.Iter(); !iter.Done(); iter.Next()) {
    auto* entry = iter.Get();
    nsPerformanceGroup* group = entry->GetKey();
    if (group->isActive()) {
      snapshot->AppendComponentsStats(GetStatsForGroup(group));
    }
  }

  js::GetPerfMonitoringTestCpuRescheduling(JS_GetRuntime(cx), &mProcessStayed, &mProcessMoved);

  if (++mProcessUpdateCounter % 10 == 0) {
    mozilla::Unused << UpdateTelemetry();
  }

  snapshot.forget(aSnapshot);

  return NS_OK;
}

uint64_t
nsPerformanceStatsService::GetNextId() {
  return ++mUIdCounter;
}

/* static*/ bool
nsPerformanceStatsService::GetPerformanceGroupsCallback(JSContext* cx, JSGroupVector& out, void* closure) {
  RefPtr<nsPerformanceStatsService> self = reinterpret_cast<nsPerformanceStatsService*>(closure);
  return self->GetPerformanceGroups(cx, out);
}

bool
nsPerformanceStatsService::GetPerformanceGroups(JSContext* cx, JSGroupVector& out) {
  JS::RootedObject global(cx, JS::CurrentGlobalOrNull(cx));
  if (!global) {
    // While it is possible for a compartment to have no global
    // (e.g. atoms), this compartment is not very interesting for us.
    return true;
  }

  // All compartments belong to the top group.
  if (!out.append(mTopGroup)) {
    JS_ReportOutOfMemory(cx);
    return false;
  }

  nsAutoString name;
  CompartmentName(cx, global, name);
  bool isSystem = nsContentUtils::IsSystemPrincipal(nsContentUtils::ObjectPrincipal(global));

  // Find out if the compartment is executed by an add-on. If so, its
  // duration should count towards the total duration of the add-on.
  JSAddonId* jsaddonId = AddonIdOfObject(global);
  nsString addonId;
  if (jsaddonId) {
    AssignJSFlatString(addonId, (JSFlatString*)jsaddonId);
    auto entry = mAddonIdToGroup.PutEntry(addonId);
    if (!entry->GetGroup()) {
      nsString addonName = name;
      addonName.AppendLiteral(" (as addon ");
      addonName.Append(addonId);
      addonName.AppendLiteral(")");
      entry->
        SetGroup(nsPerformanceGroup::Make(mRuntime, this,
                                          addonName, addonId, 0,
                                          mProcessId, isSystem,
                                          nsPerformanceGroup::GroupScope::ADDON)
                 );
    }
    if (!out.append(entry->GetGroup())) {
      JS_ReportOutOfMemory(cx);
      return false;
    }
  }

  // Find out if the compartment is executed by a window. If so, its
  // duration should count towards the total duration of the window.
  uint64_t windowId = 0;
  if (nsCOMPtr<nsPIDOMWindowOuter> ptop = GetPrivateWindow(cx)) {
    windowId = ptop->WindowID();
    auto entry = mWindowIdToGroup.PutEntry(windowId);
    if (!entry->GetGroup()) {
      nsString windowName = name;
      windowName.AppendLiteral(" (as window ");
      windowName.AppendInt(windowId);
      windowName.AppendLiteral(")");
      entry->
        SetGroup(nsPerformanceGroup::Make(mRuntime, this,
                                          windowName, EmptyString(), windowId,
                                          mProcessId, isSystem,
                                          nsPerformanceGroup::GroupScope::WINDOW)
                 );
    }
    if (!out.append(entry->GetGroup())) {
      JS_ReportOutOfMemory(cx);
      return false;
    }
  }

  // All compartments have their own group.
  auto group =
    nsPerformanceGroup::Make(mRuntime, this,
                             name, addonId, windowId,
                             mProcessId, isSystem,
                             nsPerformanceGroup::GroupScope::COMPARTMENT);
  if (!out.append(group)) {
    JS_ReportOutOfMemory(cx);
    return false;
  }

  return true;
}

/*static*/ bool
nsPerformanceStatsService::StopwatchStartCallback(uint64_t iteration, void* closure) {
  RefPtr<nsPerformanceStatsService> self = reinterpret_cast<nsPerformanceStatsService*>(closure);
  return self->StopwatchStart(iteration);
}

bool
nsPerformanceStatsService::StopwatchStart(uint64_t iteration) {
  mIteration = iteration;

  mIsHandlingUserInput = IsHandlingUserInput();
  mUserInputCount = mozilla::EventStateManager::UserInputCount();

  nsresult rv = GetResources(&mUserTimeStart, &mSystemTimeStart);
  if (NS_FAILED(rv)) {
    return false;
  }

  return true;
}

/*static*/ bool
nsPerformanceStatsService::StopwatchCommitCallback(uint64_t iteration, JSGroupVector& recentGroups, void* closure) {
  RefPtr<nsPerformanceStatsService> self = reinterpret_cast<nsPerformanceStatsService*>(closure);
  return self->StopwatchCommit(iteration, recentGroups);
}

bool
nsPerformanceStatsService::StopwatchCommit(uint64_t iteration, JSGroupVector& recentGroups)
{
  MOZ_ASSERT(iteration == mIteration);
  MOZ_ASSERT(!recentGroups.empty());

  uint64_t userTimeStop, systemTimeStop;
  nsresult rv = GetResources(&userTimeStop, &systemTimeStop);
  if (NS_FAILED(rv)) {
    return false;
  }

  // `GetResources` is not guaranteed to be monotonic, so round up
  // any negative result to 0 milliseconds.
  uint64_t userTimeDelta = 0;
  if (userTimeStop > mUserTimeStart)
    userTimeDelta = userTimeStop - mUserTimeStart;

  uint64_t systemTimeDelta = 0;
  if (systemTimeStop > mSystemTimeStart)
    systemTimeDelta = systemTimeStop - mSystemTimeStart;

  MOZ_ASSERT(mTopGroup->isUsedInThisIteration());
  const uint64_t totalRecentCycles = mTopGroup->recentCycles(iteration);

  const bool isHandlingUserInput = mIsHandlingUserInput || mozilla::EventStateManager::UserInputCount() > mUserInputCount;

  // We should only reach this stage if `group` has had some activity.
  MOZ_ASSERT(mTopGroup->recentTicks(iteration) > 0);
  for (auto iter = recentGroups.begin(), end = recentGroups.end(); iter != end; ++iter) {
    RefPtr<nsPerformanceGroup> group = nsPerformanceGroup::Get(*iter);
    CommitGroup(iteration, userTimeDelta, systemTimeDelta, totalRecentCycles, isHandlingUserInput, group);
  }

  // Make sure that `group` was treated along with the other items of `recentGroups`.
  MOZ_ASSERT(!mTopGroup->isUsedInThisIteration());
  MOZ_ASSERT(mTopGroup->recentTicks(iteration) == 0);

  if (!mPendingAlerts.empty()) {
    mPendingAlertsCollector->Start(mJankAlertBufferingDelay);
  }

  return true;
}

void
nsPerformanceStatsService::CommitGroup(uint64_t iteration,
                                       uint64_t totalUserTimeDelta, uint64_t totalSystemTimeDelta,
                                       uint64_t totalCyclesDelta,
                                       bool isHandlingUserInput,
                                       nsPerformanceGroup* group) {

  MOZ_ASSERT(group->isUsedInThisIteration());

  const uint64_t ticksDelta = group->recentTicks(iteration);
  const uint64_t cpowTimeDelta = group->recentCPOW(iteration);
  const uint64_t cyclesDelta = group->recentCycles(iteration);
  group->resetRecentData();

  // We have now performed all cleanup and may `return` at any time without fear of leaks.

  if (group->iteration() != iteration) {
    // Stale data, don't commit.
    return;
  }

  // When we add a group as changed, we immediately set its
  // `recentTicks` from 0 to 1.  If we have `ticksDelta == 0` at
  // this stage, we have already called `resetRecentData` but we
  // haven't removed it from the list.
  MOZ_ASSERT(ticksDelta != 0);
  MOZ_ASSERT(cyclesDelta <= totalCyclesDelta);
  if (cyclesDelta == 0 || totalCyclesDelta == 0) {
    // Nothing useful, don't commit.
    return;
  }

  double proportion = (double)cyclesDelta / (double)totalCyclesDelta;
  MOZ_ASSERT(proportion <= 1);

  const uint64_t userTimeDelta = proportion * totalUserTimeDelta;
  const uint64_t systemTimeDelta = proportion * totalSystemTimeDelta;

  group->data.mTotalUserTime += userTimeDelta;
  group->data.mTotalSystemTime += systemTimeDelta;
  group->data.mTotalCPOWTime += cpowTimeDelta;
  group->data.mTicks += ticksDelta;

  const uint64_t totalTimeDelta = userTimeDelta + systemTimeDelta + cpowTimeDelta;
  uint64_t duration = 1000;   // 1ms in µs
  for (size_t i = 0;
       i < mozilla::ArrayLength(group->data.mDurations) && duration < totalTimeDelta;
       ++i, duration *= 2) {
    group->data.mDurations[i]++;
  }

  group->RecordJank(totalTimeDelta);
  group->RecordCPOW(cpowTimeDelta);
  if (isHandlingUserInput) {
    group->RecordUserInput();
  }

  if (totalTimeDelta >= mJankAlertThreshold) {
    if (!group->HasPendingAlert()) {
      if (mPendingAlerts.append(group)) {
        group->SetHasPendingAlert(true);
      }
      return;
    }
  }

  return;
}

nsresult
nsPerformanceStatsService::GetResources(uint64_t* userTime,
                                        uint64_t* systemTime) const {
  MOZ_ASSERT(userTime);
  MOZ_ASSERT(systemTime);

#if defined(XP_MACOSX)
  // On MacOS X, to get we per-thread data, we need to
  // reach into the kernel.

  mach_msg_type_number_t count = THREAD_BASIC_INFO_COUNT;
  thread_basic_info_data_t info;
  mach_port_t port = mach_thread_self();
  kern_return_t err =
    thread_info(/* [in] targeted thread*/ port,
                /* [in] nature of information*/ THREAD_BASIC_INFO,
                /* [out] thread information */  (thread_info_t)&info,
                /* [inout] number of items */   &count);

  // We do not need ability to communicate with the thread, so
  // let's release the port.
  mach_port_deallocate(mach_task_self(), port);

  if (err != KERN_SUCCESS)
    return NS_ERROR_FAILURE;

  *userTime = info.user_time.microseconds + info.user_time.seconds * 1000000;
  *systemTime = info.system_time.microseconds + info.system_time.seconds * 1000000;

#elif defined(XP_UNIX)
  struct rusage rusage;
#if defined(RUSAGE_THREAD)
  // Under Linux, we can obtain per-thread statistics
  int err = getrusage(RUSAGE_THREAD, &rusage);
#else
  // Under other Unices, we need to do with more noisy
  // per-process statistics.
  int err = getrusage(RUSAGE_SELF, &rusage);
#endif // defined(RUSAGE_THREAD)

  if (err)
    return NS_ERROR_FAILURE;

  *userTime = rusage.ru_utime.tv_usec + rusage.ru_utime.tv_sec * 1000000;
  *systemTime = rusage.ru_stime.tv_usec + rusage.ru_stime.tv_sec * 1000000;

#elif defined(XP_WIN)
  // Under Windows, we can obtain per-thread statistics. Experience
  // seems to suggest that they are not very accurate under Windows
  // XP, though.
  FILETIME creationFileTime; // Ignored
  FILETIME exitFileTime; // Ignored
  FILETIME kernelFileTime;
  FILETIME userFileTime;
  BOOL success = GetThreadTimes(GetCurrentThread(),
                                &creationFileTime, &exitFileTime,
                                &kernelFileTime, &userFileTime);

  if (!success)
    return NS_ERROR_FAILURE;

  ULARGE_INTEGER kernelTimeInt;
  kernelTimeInt.LowPart = kernelFileTime.dwLowDateTime;
  kernelTimeInt.HighPart = kernelFileTime.dwHighDateTime;
  // Convert 100 ns to 1 us.
  *systemTime = kernelTimeInt.QuadPart / 10;

  ULARGE_INTEGER userTimeInt;
  userTimeInt.LowPart = userFileTime.dwLowDateTime;
  userTimeInt.HighPart = userFileTime.dwHighDateTime;
  // Convert 100 ns to 1 us.
  *userTime = userTimeInt.QuadPart / 10;

#endif // defined(XP_MACOSX) || defined(XP_UNIX) || defined(XP_WIN)

  return NS_OK;
}

void
nsPerformanceStatsService::NotifyJankObservers(const mozilla::Vector<uint64_t>& aPreviousJankLevels) {
  GroupVector alerts;
  mPendingAlerts.swap(alerts);
  if (!mPendingAlertsCollector) {
    // We are shutting down.
    return;
  }

  // Find out if we have noticed any user-noticeable delay in an
  // animation recently (i.e. since the start of the execution of JS
  // code that caused this collector to start). If so, we'll mark any
  // alert as part of a user-noticeable jank. Note that this doesn't
  // mean with any certainty that the alert is the only cause of jank,
  // or even the main cause of jank.
  mozilla::Vector<uint64_t> latestJankLevels;
  {
    mozilla::DebugOnly<bool> result = nsRefreshDriver::GetJankLevels(latestJankLevels);
    MOZ_ASSERT(result);
  }
  MOZ_ASSERT(latestJankLevels.length() == aPreviousJankLevels.length());

  bool isJankInAnimation = false;
  for (size_t i = mJankLevelVisibilityThreshold; i < latestJankLevels.length(); ++i) {
    if (latestJankLevels[i] > aPreviousJankLevels[i]) {
      isJankInAnimation = true;
      break;
    }
  }

  MOZ_ASSERT(!alerts.empty());
  const bool hasUniversalAddonObservers = mUniversalTargets.mAddons->HasObservers();
  const bool hasUniversalWindowObservers = mUniversalTargets.mWindows->HasObservers();
  for (auto iter = alerts.begin(); iter < alerts.end(); ++iter) {
    MOZ_ASSERT(iter);
    RefPtr<nsPerformanceGroup> group = *iter;
    group->SetHasPendingAlert(false);

    RefPtr<nsPerformanceGroupDetails> details = group->Details();
    nsPerformanceObservationTarget* targets[3] = {
      hasUniversalAddonObservers && details->IsAddon() ? mUniversalTargets.mAddons.get() : nullptr,
      hasUniversalWindowObservers && details->IsWindow() ? mUniversalTargets.mWindows.get() : nullptr,
      group->ObservationTarget()
    };

    bool isJankInInput = group->HasRecentUserInput();

    RefPtr<PerformanceAlert> alert;
    for (nsPerformanceObservationTarget* target : targets) {
      if (!target) {
        continue;
      }
      if (!alert) {
        const uint32_t reason = nsIPerformanceAlert::REASON_SLOWDOWN
          | (isJankInAnimation ? nsIPerformanceAlert::REASON_JANK_IN_ANIMATION : 0)
          | (isJankInInput ? nsIPerformanceAlert::REASON_JANK_IN_INPUT : 0);
        // Wait until we are sure we need to allocate before we allocate.
        alert = new PerformanceAlert(reason, group);
      }
      target->NotifyJankObservers(details, alert);
    }

    group->ResetRecent();
  }

}

NS_IMETHODIMP
nsPerformanceStatsService::GetObservableAddon(const nsAString& addonId,
                                              nsIPerformanceObservable** result) {
  if (addonId.Equals(NS_LITERAL_STRING("*"))) {
    NS_IF_ADDREF(*result = mUniversalTargets.mAddons);
  } else {
    auto entry = mAddonIdToGroup.PutEntry(addonId);
    NS_IF_ADDREF(*result = entry->ObservationTarget());
  }
  return NS_OK;
}

NS_IMETHODIMP
nsPerformanceStatsService::GetObservableWindow(uint64_t windowId,
                                               nsIPerformanceObservable** result) {
  if (windowId == 0) {
    NS_IF_ADDREF(*result = mUniversalTargets.mWindows);
  } else {
    auto entry = mWindowIdToGroup.PutEntry(windowId);
    NS_IF_ADDREF(*result = entry->ObservationTarget());
  }
  return NS_OK;
}

NS_IMETHODIMP
nsPerformanceStatsService::GetAnimationJankLevelThreshold(short* result) {
  *result = mJankLevelVisibilityThreshold;
  return NS_OK;
}

NS_IMETHODIMP
nsPerformanceStatsService::SetAnimationJankLevelThreshold(short value) {
  mJankLevelVisibilityThreshold = value;
  return NS_OK;
}

NS_IMETHODIMP
nsPerformanceStatsService::GetUserInputDelayThreshold(uint64_t* result) {
  *result = mMaxExpectedDurationOfInteractionUS;
  return NS_OK;
}

NS_IMETHODIMP
nsPerformanceStatsService::SetUserInputDelayThreshold(uint64_t value) {
  mMaxExpectedDurationOfInteractionUS = value;
  return NS_OK;
}



nsPerformanceStatsService::UniversalTargets::UniversalTargets()
  : mAddons(new nsPerformanceObservationTarget())
  , mWindows(new nsPerformanceObservationTarget())
{ }

/* ------------------------------------------------------
 *
 * Class nsPerformanceGroup
 *
 */

/*static*/ nsPerformanceGroup*
nsPerformanceGroup::Make(JSRuntime* rt,
                         nsPerformanceStatsService* service,
                         const nsAString& name,
                         const nsAString& addonId,
                         uint64_t windowId,
                         uint64_t processId,
                         bool isSystem,
                         GroupScope scope)
{
  nsString groupId;
  ::GenerateUniqueGroupId(rt, service->GetNextId(), processId, groupId);
  return new nsPerformanceGroup(service, name, groupId, addonId, windowId, processId, isSystem, scope);
}

nsPerformanceGroup::nsPerformanceGroup(nsPerformanceStatsService* service,
                                       const nsAString& name,
                                       const nsAString& groupId,
                                       const nsAString& addonId,
                                       uint64_t windowId,
                                       uint64_t processId,
                                       bool isSystem,
                                       GroupScope scope)
  : mDetails(new nsPerformanceGroupDetails(name, groupId, addonId, windowId, processId, isSystem))
  , mService(service)
  , mScope(scope)
  , mHighestJank(0)
  , mHighestCPOW(0)
  , mHasRecentUserInput(false)
  , mHasPendingAlert(false)
{
  mozilla::Unused << mService->mGroups.PutEntry(this);

#if defined(DEBUG)
  if (scope == GroupScope::ADDON) {
    MOZ_ASSERT(mDetails->IsAddon());
    MOZ_ASSERT(!mDetails->IsWindow());
  } else if (scope == GroupScope::WINDOW) {
    MOZ_ASSERT(mDetails->IsWindow());
    MOZ_ASSERT(!mDetails->IsAddon());
  } else if (scope == GroupScope::RUNTIME) {
    MOZ_ASSERT(!mDetails->IsWindow());
    MOZ_ASSERT(!mDetails->IsAddon());
  }
#endif // defined(DEBUG)
  setIsActive(mScope != GroupScope::COMPARTMENT || mService->mIsMonitoringPerCompartment);
}

void
nsPerformanceGroup::Dispose() {
  if (!mService) {
    // We have already called `Dispose()`.
    return;
  }
  if (mObservationTarget) {
    mObservationTarget = nullptr;
  }

  // Remove any reference to the service.
  RefPtr<nsPerformanceStatsService> service;
  service.swap(mService);

  // Remove any dangling pointer to `this`.
  service->mGroups.RemoveEntry(this);

  if (mScope == GroupScope::ADDON) {
    MOZ_ASSERT(mDetails->IsAddon());
    service->mAddonIdToGroup.RemoveEntry(mDetails->AddonId());
  } else if (mScope == GroupScope::WINDOW) {
    MOZ_ASSERT(mDetails->IsWindow());
    service->mWindowIdToGroup.RemoveEntry(mDetails->WindowId());
  }
}

nsPerformanceGroup::~nsPerformanceGroup() {
  Dispose();
}

nsPerformanceGroup::GroupScope
nsPerformanceGroup::Scope() const {
  return mScope;
}

nsPerformanceGroupDetails*
nsPerformanceGroup::Details() const {
  return mDetails;
}

void
nsPerformanceGroup::SetObservationTarget(nsPerformanceObservationTarget* target) {
  MOZ_ASSERT(!mObservationTarget);
  mObservationTarget = target;
}

nsPerformanceObservationTarget*
nsPerformanceGroup::ObservationTarget() const {
  return mObservationTarget;
}

bool
nsPerformanceGroup::HasPendingAlert() const {
  return mHasPendingAlert;
}

void
nsPerformanceGroup::SetHasPendingAlert(bool value) {
  mHasPendingAlert = value;
}


void
nsPerformanceGroup::RecordJank(uint64_t jank) {
  if (jank > mHighestJank) {
    mHighestJank = jank;
  }
}

void
nsPerformanceGroup::RecordCPOW(uint64_t cpow) {
  if (cpow > mHighestCPOW) {
    mHighestCPOW = cpow;
  }
}

uint64_t
nsPerformanceGroup::HighestRecentJank() {
  return mHighestJank;
}

uint64_t
nsPerformanceGroup::HighestRecentCPOW() {
  return mHighestCPOW;
}

bool
nsPerformanceGroup::HasRecentUserInput() {
  return mHasRecentUserInput;
}

void
nsPerformanceGroup::RecordUserInput() {
  mHasRecentUserInput = true;
}

void
nsPerformanceGroup::ResetRecent() {
  mHighestJank = 0;
  mHighestCPOW = 0;
  mHasRecentUserInput = false;
}
