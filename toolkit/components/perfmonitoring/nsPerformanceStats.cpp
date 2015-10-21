/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPerformanceStats.h"

#include "nsMemory.h"
#include "nsLiteralString.h"
#include "nsCRTGlue.h"
#include "nsServiceManagerUtils.h"

#include "nsCOMArray.h"
#include "nsIMutableArray.h"
#include "nsReadableUtils.h"

#include "jsapi.h"
#include "nsJSUtils.h"
#include "xpcpublic.h"
#include "jspubtd.h"

#include "nsIDOMWindow.h"
#include "nsGlobalWindow.h"

#include "mozilla/unused.h"
#include "mozilla/ArrayUtils.h"
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
#endif // defined(XP_MACOSX)

#if defined(XP_LINUX)
#include <sys/time.h>
#include <sys/resource.h>
#endif // defined(XP_LINUX)
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
already_AddRefed<nsPIDOMWindow>
GetPrivateWindow(JSContext* cx) {
  nsCOMPtr<nsPIDOMWindow> win = xpc::CurrentWindowOrNull(cx);
  if (!win) {
    return nullptr;
  }

  nsCOMPtr<nsIDOMWindow> top;
  nsresult rv = win->GetTop(getter_AddRefs(top));
  if (!top || NS_FAILED(rv)) {
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindow> ptop = do_QueryInterface(top);
  if (!ptop) {
    return nullptr;
  }
  return ptop.forget();
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

} // namespace



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

/* ------------------------------------------------------
 *
 * struct nsPerformanceStats
 *
 */

class nsPerformanceStats: public nsIPerformanceStats,
                          public nsPerformanceGroupDetails
{
public:
  nsPerformanceStats(const nsAString& aName,
                     const nsAString& aGroupId,
                     const nsAString& aAddonId,
                     const uint64_t aWindowId,
                     const uint64_t aProcessId,
                     const bool aIsSystem,
                     const PerformanceData& aPerformanceData)
    : nsPerformanceGroupDetails(aName, aGroupId, aAddonId, aWindowId, aProcessId, aIsSystem)
    , mPerformanceData(aPerformanceData)
  {
  }
  nsPerformanceStats(const nsPerformanceGroupDetails& item,
                     const PerformanceData& aPerformanceData)
    : nsPerformanceGroupDetails(item)
    , mPerformanceData(aPerformanceData)
  {
  }

  NS_DECL_ISUPPORTS

  /* readonly attribute AString name; */
  NS_IMETHOD GetName(nsAString& aName) override {
    aName.Assign(nsPerformanceGroupDetails::Name());
    return NS_OK;
  };

  /* readonly attribute AString groupId; */
  NS_IMETHOD GetGroupId(nsAString& aGroupId) override {
    aGroupId.Assign(nsPerformanceGroupDetails::GroupId());
    return NS_OK;
  };

  /* readonly attribute AString addonId; */
  NS_IMETHOD GetAddonId(nsAString& aAddonId) override {
    aAddonId.Assign(nsPerformanceGroupDetails::AddonId());
    return NS_OK;
  };

  /* readonly attribute uint64_t windowId; */
  NS_IMETHOD GetWindowId(uint64_t *aWindowId) override {
    *aWindowId = nsPerformanceGroupDetails::WindowId();
    return NS_OK;
  }

  /* readonly attribute bool isSystem; */
  NS_IMETHOD GetIsSystem(bool *_retval) override {
    *_retval = nsPerformanceGroupDetails::IsSystem();
    return NS_OK;
  }

  /* readonly attribute unsigned long long totalUserTime; */
  NS_IMETHOD GetTotalUserTime(uint64_t *aTotalUserTime) override {
    *aTotalUserTime = mPerformanceData.mTotalUserTime;
    return NS_OK;
  };

  /* readonly attribute unsigned long long totalSystemTime; */
  NS_IMETHOD GetTotalSystemTime(uint64_t *aTotalSystemTime) override {
    *aTotalSystemTime = mPerformanceData.mTotalSystemTime;
    return NS_OK;
  };

  /* readonly attribute unsigned long long totalCPOWTime; */
  NS_IMETHOD GetTotalCPOWTime(uint64_t *aCpowTime) override {
    *aCpowTime = mPerformanceData.mTotalCPOWTime;
    return NS_OK;
  };

  /* readonly attribute unsigned long long ticks; */
  NS_IMETHOD GetTicks(uint64_t *aTicks) override {
    *aTicks = mPerformanceData.mTicks;
    return NS_OK;
  };

  /* void getDurations (out unsigned long aCount, [array, size_is (aCount), retval] out unsigned long long aNumberOfOccurrences); */
  NS_IMETHOD GetDurations(uint32_t *aCount, uint64_t **aNumberOfOccurrences) override {
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

  /*
    readonly attribute unsigned long long processId;
  */
  NS_IMETHODIMP GetProcessId(uint64_t* processId) override {
    *processId = nsPerformanceGroupDetails::ProcessId();
    return NS_OK;
  }

private:
  PerformanceData mPerformanceData;

  virtual ~nsPerformanceStats() {}
};

NS_IMPL_ISUPPORTS(nsPerformanceStats, nsIPerformanceStats)


/* ------------------------------------------------------
 *
 * struct nsPerformanceSnapshot
 *
 */

class nsPerformanceSnapshot : public nsIPerformanceSnapshot
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
  virtual ~nsPerformanceSnapshot() {}

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
 * class nsPerformanceStatsService
 *
 */

NS_IMPL_ISUPPORTS(nsPerformanceStatsService, nsIPerformanceStatsService, nsIObserver)

nsPerformanceStatsService::nsPerformanceStatsService()
#if defined(XP_WIN)
  : mProcessId(GetCurrentProcessId())
#else
  : mProcessId(getpid())
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
  , mProcessStayed(0)
  , mProcessMoved(0)
  , mProcessUpdateCounter(0)
  , mIsMonitoringPerCompartment(false)

{ }

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

  // Disconnect from nsIObserverService.
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->RemoveObserver(this, "profile-before-change");
    obs->RemoveObserver(this, "quit-application");
    obs->RemoveObserver(this, "quit-application-granted");
    obs->RemoveObserver(this, "content-child-shutdown");
  }

  // Clear up and disconnect from JSAPI.
  js::DisposePerformanceMonitoring(mRuntime);

  mozilla::unused << js::SetStopwatchIsMonitoringCPOW(mRuntime, false);
  mozilla::unused << js::SetStopwatchIsMonitoringJank(mRuntime, false);

  mozilla::unused << js::SetStopwatchStartCallback(mRuntime, nullptr, nullptr);
  mozilla::unused << js::SetStopwatchCommitCallback(mRuntime, nullptr, nullptr);
  mozilla::unused << js::SetGetPerformanceGroupsCallback(mRuntime, nullptr, nullptr);

  // At this stage, the JS VM may still be holding references to
  // instances of PerformanceGroup on the stack. To let the service be
  // collected, we need to break the references from these groups to
  // `this`.
  mTopGroup->Dispose();

  // Copy references to the groups to a vector to ensure that we do
  // not modify the hashtable while iterating it.
  GroupVector groups;
  for (auto iter = mGroups.Iter(); !iter.Done(); iter.Next()) {
    groups.append(iter.Get()->GetKey());
  }
  for (auto iter = groups.begin(); iter < groups.end(); iter++) {
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
    obs->AddObserver(this, "profile-before-change", false);
    obs->AddObserver(this, "quit-application-granted", false);
    obs->AddObserver(this, "quit-application", false);
    obs->AddObserver(this, "content-child-shutdown", false);
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
             || strcmp(aTopic, "content-child-shutdown") == 0);

  Dispose();
  return NS_OK;
}

/* [implicit_jscontext] attribute bool isMonitoringCPOW; */
NS_IMETHODIMP
nsPerformanceStatsService::GetIsMonitoringCPOW(JSContext* cx, bool *aIsStopwatchActive)
{
  JSRuntime *runtime = JS_GetRuntime(cx);
  *aIsStopwatchActive = js::GetStopwatchIsMonitoringCPOW(runtime);
  return NS_OK;
}
NS_IMETHODIMP
nsPerformanceStatsService::SetIsMonitoringCPOW(JSContext* cx, bool aIsStopwatchActive)
{
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
  JSRuntime *runtime = JS_GetRuntime(cx);
  *aIsStopwatchActive = js::GetStopwatchIsMonitoringJank(runtime);
  return NS_OK;
}
NS_IMETHODIMP
nsPerformanceStatsService::SetIsMonitoringJank(JSContext* cx, bool aIsStopwatchActive)
{
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
  *aIsMonitoringPerCompartment = mIsMonitoringPerCompartment;
  return NS_OK;
}
NS_IMETHODIMP
nsPerformanceStatsService::SetIsMonitoringPerCompartment(JSContext*, bool aIsMonitoringPerCompartment)
{
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
  return new nsPerformanceStats(*group, group->data);
}

/* [implicit_jscontext] nsIPerformanceSnapshot getSnapshot (); */
NS_IMETHODIMP
nsPerformanceStatsService::GetSnapshot(JSContext* cx, nsIPerformanceSnapshot * *aSnapshot)
{
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
    mozilla::unused << UpdateTelemetry();
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
  out.append(mTopGroup);

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
    if (!entry->mGroup) {
      nsString addonName = name;
      addonName.AppendLiteral(" (as addon ");
      addonName.Append(addonId);
      addonName.AppendLiteral(")");
      entry->mGroup =
        nsPerformanceGroup::Make(mRuntime, this,
                                 addonName, addonId, 0,
                                 mProcessId, isSystem,
                                 nsPerformanceGroup::GroupScope::ADDON);
    }
    out.append(entry->mGroup);
  }

  // Find out if the compartment is executed by a window. If so, its
  // duration should count towards the total duration of the window.
  nsCOMPtr<nsPIDOMWindow> ptop = GetPrivateWindow(cx);
  uint64_t windowId = 0;
  if (ptop) {
    windowId = ptop->WindowID();
    auto entry = mWindowIdToGroup.PutEntry(windowId);
    if (!entry->mGroup) {
      nsString windowName = name;
      windowName.AppendLiteral(" (as window ");
      windowName.AppendInt(windowId);
      windowName.AppendLiteral(")");
      entry->mGroup =
        nsPerformanceGroup::Make(mRuntime, this,
                                 windowName, EmptyString(), windowId,
                                 mProcessId, isSystem,
                                 nsPerformanceGroup::GroupScope::WINDOW);
    }
    out.append(entry->mGroup);
  }

  // All compartments have their own group.
  auto group =
    nsPerformanceGroup::Make(mRuntime, this,
                             name, addonId, windowId,
                             mProcessId, isSystem,
                             nsPerformanceGroup::GroupScope::COMPARTMENT);
  out.append(group);

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
  MOZ_ASSERT(recentGroups.length() > 0);

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

  // We should only reach this stage if `group` has had some activity.
  MOZ_ASSERT(mTopGroup->recentTicks(iteration) > 0);
  for (auto iter = recentGroups.begin(); iter != recentGroups.end(); ++iter) {
    RefPtr<nsPerformanceGroup> group = nsPerformanceGroup::Get(*iter);
    CommitGroup(iteration, userTimeDelta, systemTimeDelta, totalRecentCycles, group);
  }

  // Make sure that `group` was treated along with the other items of `recentGroups`.
  MOZ_ASSERT(!mTopGroup->isUsedInThisIteration());
  MOZ_ASSERT(mTopGroup->recentTicks(iteration) == 0);
  return true;
}

void
nsPerformanceStatsService::CommitGroup(uint64_t iteration,
                                       uint64_t totalUserTimeDelta, uint64_t totalSystemTimeDelta,
                                       uint64_t totalCyclesDelta, nsPerformanceGroup* group) {

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

  const uint64_t totalTimeDelta = userTimeDelta + systemTimeDelta;
  uint64_t duration = 1000;   // 1ms in µs
  for (size_t i = 0;
       i < mozilla::ArrayLength(group->data.mDurations) && duration < totalTimeDelta;
       ++i, duration *= 2) {
    group->data.mDurations[i]++;
  }
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
  : nsPerformanceGroupDetails(name, groupId, addonId, windowId, processId, isSystem)
  , mService(service)
  , mScope(scope)
{
  mozilla::unused << mService->mGroups.PutEntry(this);

#if defined(DEBUG)
  if (scope == GroupScope::ADDON) {
    MOZ_ASSERT(IsAddon());
    MOZ_ASSERT(!IsWindow());
  } else if (scope == GroupScope::WINDOW) {
    MOZ_ASSERT(IsWindow());
    MOZ_ASSERT(!IsAddon());
  } else if (scope == GroupScope::RUNTIME) {
    MOZ_ASSERT(!IsWindow());
    MOZ_ASSERT(!IsAddon());
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

  // Remove any reference to the service
  RefPtr<nsPerformanceStatsService> service;
  service.swap(mService);

  service->mGroups.RemoveEntry(this);

  if (mScope == GroupScope::ADDON) {
    MOZ_ASSERT(IsAddon());
    service->mAddonIdToGroup.RemoveEntry(AddonId());
  } else if (mScope == GroupScope::WINDOW) {
    MOZ_ASSERT(IsWindow());
    service->mWindowIdToGroup.RemoveEntry(WindowId());
  }
}

nsPerformanceGroup::~nsPerformanceGroup() {
  Dispose();
}

nsPerformanceGroup::GroupScope
nsPerformanceGroup::Scope() const {
  return mScope;
}
