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

#include "jsapi.h"
#include "nsJSUtils.h"
#include "xpcpublic.h"
#include "jspubtd.h"

#include "nsIDOMWindow.h"
#include "nsGlobalWindow.h"

#include "mozilla/unused.h"
#include "mozilla/Services.h"
#include "mozilla/Telemetry.h"

#if defined(XP_WIN)
#include "windows.h"
#else
#include <unistd.h>
#endif

class nsPerformanceStats: public nsIPerformanceStats {
public:
  nsPerformanceStats(const nsAString& aName,
                     nsIPerformanceStats* aParent,
                     const nsAString& aGroupId,
                     const nsAString& aAddonId,
                     const nsAString& aTitle,
                     const uint64_t aWindowId,
                     const uint64_t aProcessId,
                     const bool aIsSystem,
                     const js::PerformanceData& aPerformanceData)
    : mName(aName)
    , mGroupId(aGroupId)
    , mAddonId(aAddonId)
    , mTitle(aTitle)
    , mWindowId(aWindowId)
    , mProcessId(aProcessId)
    , mIsSystem(aIsSystem)
    , mPerformanceData(aPerformanceData)
  {
    if (aParent) {
      mozilla::DebugOnly<nsresult> rv = aParent->GetGroupId(mParentId);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }
  }
  explicit nsPerformanceStats() {}

  NS_DECL_ISUPPORTS

  /* readonly attribute AString name; */
  NS_IMETHOD GetName(nsAString& aName) override {
    aName.Assign(mName);
    return NS_OK;
  };

  /* readonly attribute AString groupId; */
  NS_IMETHOD GetGroupId(nsAString& aGroupId) override {
    aGroupId.Assign(mGroupId);
    return NS_OK;
  };

  /* readonly attribute AString parentId; */
  NS_IMETHOD GetParentId(nsAString& aParentId) override {
    aParentId.Assign(mParentId);
    return NS_OK;
  };

  /* readonly attribute AString addonId; */
  NS_IMETHOD GetAddonId(nsAString& aAddonId) override {
    aAddonId.Assign(mAddonId);
    return NS_OK;
  };

  /* readonly attribute uint64_t windowId; */
  NS_IMETHOD GetWindowId(uint64_t *aWindowId) override {
    *aWindowId = mWindowId;
    return NS_OK;
  }

  /* readonly attribute AString title; */
  NS_IMETHOD GetTitle(nsAString & aTitle) override {
    aTitle.Assign(mTitle);
    return NS_OK;
  }

  /* readonly attribute bool isSystem; */
  NS_IMETHOD GetIsSystem(bool *_retval) override {
    *_retval = mIsSystem;
    return NS_OK;
  }

  /* readonly attribute unsigned long long totalUserTime; */
  NS_IMETHOD GetTotalUserTime(uint64_t *aTotalUserTime) override {
    *aTotalUserTime = mPerformanceData.totalUserTime;
    return NS_OK;
  };

  /* readonly attribute unsigned long long totalSystemTime; */
  NS_IMETHOD GetTotalSystemTime(uint64_t *aTotalSystemTime) override {
    *aTotalSystemTime = mPerformanceData.totalSystemTime;
    return NS_OK;
  };

  /* readonly attribute unsigned long long totalCPOWTime; */
  NS_IMETHOD GetTotalCPOWTime(uint64_t *aCpowTime) override {
    *aCpowTime = mPerformanceData.totalCPOWTime;
    return NS_OK;
  };

  /* readonly attribute unsigned long long ticks; */
  NS_IMETHOD GetTicks(uint64_t *aTicks) override {
    *aTicks = mPerformanceData.ticks;
    return NS_OK;
  };

  /* void getDurations (out unsigned long aCount, [array, size_is (aCount), retval] out unsigned long long aNumberOfOccurrences); */
  NS_IMETHOD GetDurations(uint32_t *aCount, uint64_t **aNumberOfOccurrences) override {
    const size_t length = mozilla::ArrayLength(mPerformanceData.durations);
    if (aCount) {
      *aCount = length;
    }
    *aNumberOfOccurrences = new uint64_t[length];
    for (size_t i = 0; i < length; ++i) {
      (*aNumberOfOccurrences)[i] = mPerformanceData.durations[i];
    }
    return NS_OK;
  };

  /*
    readonly attribute unsigned long long processId;
  */
  NS_IMETHODIMP GetProcessId(uint64_t* processId) override {
    *processId = mProcessId;
    return NS_OK;
  }

private:
  nsString mName;
  nsString mParentId;
  nsString mGroupId;
  nsString mAddonId;
  nsString mTitle;
  uint64_t mWindowId;
  uint64_t mProcessId;
  bool mIsSystem;

  js::PerformanceData mPerformanceData;

  virtual ~nsPerformanceStats() {}
};

NS_IMPL_ISUPPORTS(nsPerformanceStats, nsIPerformanceStats)


class nsPerformanceSnapshot : public nsIPerformanceSnapshot
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPERFORMANCESNAPSHOT

  nsPerformanceSnapshot();
  nsresult Init(JSContext*, uint64_t processId);
private:
  virtual ~nsPerformanceSnapshot();

  /**
   * Import a `js::PerformanceStats` as a `nsIPerformanceStats`.
   *
   * Precondition: this method assumes that we have entered the JSCompartment for which data `c`
   * has been collected.
   *
   * `cx` may be `nullptr` if we are importing the statistics for the
   * entire process, rather than the statistics for a specific set of
   * compartments.
   */
  already_AddRefed<nsIPerformanceStats> ImportStats(JSContext* cx, const js::PerformanceData& data, uint64_t uid, nsIPerformanceStats* parent);

  /**
   * Callbacks for iterating through the `PerformanceStats` of a runtime.
   */
  bool IterPerformanceStatsCallbackInternal(JSContext* cx,
                                            const js::PerformanceData& ownStats, const uint64_t ownId,
                                            const uint64_t* parentId);
  static bool IterPerformanceStatsCallback(JSContext* cx,
                                           const js::PerformanceData& ownStats, const uint64_t ownId,
                                           const uint64_t* parentId,
                                           void* self);

  // If the context represents a window, extract the title and window ID.
  // Otherwise, extract "" and 0.
  static void GetWindowData(JSContext*,
                            nsString& title,
                            uint64_t* windowId);
  void GetGroupId(JSContext*,
                  uint64_t uid,
                  nsString& groupId);

  static void GetName(JSContext*,
                      JS::Handle<JSObject*> global,
                      nsString& name);

  // If the context presents an add-on, extract the addon ID.
  // Otherwise, extract "".
  static void GetAddonId(JSContext*,
                         JS::Handle<JSObject*> global,
                         nsAString& addonId);

  // Determine whether a context is part of the system principals.
  static bool GetIsSystem(JSContext*,
                          JS::Handle<JSObject*> global);

private:
  nsCOMArray<nsIPerformanceStats> mComponentsData;
  nsCOMPtr<nsIPerformanceStats> mProcessData;
  nsBaseHashtable<nsUint64HashKey, nsCOMPtr<nsIPerformanceStats>, nsCOMPtr<nsIPerformanceStats> > mCachedStats;
  uint64_t mProcessId;
};

NS_IMPL_ISUPPORTS(nsPerformanceSnapshot, nsIPerformanceSnapshot)

nsPerformanceSnapshot::nsPerformanceSnapshot()
{
}

nsPerformanceSnapshot::~nsPerformanceSnapshot()
{
}

/* static */ void
nsPerformanceSnapshot::GetWindowData(JSContext* cx,
                                     nsString& title,
                                     uint64_t* windowId)
{
  MOZ_ASSERT(windowId);

  title.SetIsVoid(true);
  *windowId = 0;

  nsCOMPtr<nsPIDOMWindow> win = xpc::CurrentWindowOrNull(cx);
  if (!win) {
    return;
  }

  nsCOMPtr<nsIDOMWindow> top;
  nsresult rv = win->GetTop(getter_AddRefs(top));
  if (!top || NS_FAILED(rv)) {
    return;
  }

  nsCOMPtr<nsPIDOMWindow> ptop = do_QueryInterface(top);
  if (!ptop) {
    return;
  }

  nsCOMPtr<nsIDocument> doc = ptop->GetExtantDoc();
  if (!doc) {
    return;
  }

  doc->GetTitle(title);
  *windowId = ptop->WindowID();
}

/* static */ void
nsPerformanceSnapshot::GetAddonId(JSContext*,
                                  JS::Handle<JSObject*> global,
                                  nsAString& addonId)
{
  addonId.AssignLiteral("");

  JSAddonId* jsid = AddonIdOfObject(global);
  if (!jsid) {
    return;
  }
  AssignJSFlatString(addonId, (JSFlatString*)jsid);
}

void
nsPerformanceSnapshot::GetGroupId(JSContext* cx,
                                  uint64_t uid,
                                  nsString& groupId)
{
  JSRuntime* rt = JS_GetRuntime(cx);
  uint64_t runtimeId = reinterpret_cast<uintptr_t>(rt);

  groupId.AssignLiteral("process: ");
  groupId.AppendInt(mProcessId);
  groupId.AppendLiteral(", thread: ");
  groupId.AppendInt(runtimeId);
  groupId.AppendLiteral(", group: ");
  groupId.AppendInt(uid);
}

/* static */ bool
nsPerformanceSnapshot::GetIsSystem(JSContext*,
                                   JS::Handle<JSObject*> global)
{
  return nsContentUtils::IsSystemPrincipal(nsContentUtils::ObjectPrincipal(global));
}

/* static */
void
nsPerformanceSnapshot::GetName(JSContext* cx,
                               JS::Handle<JSObject*> global,
                               nsString& name)
{
  nsAutoCString cname;
  do {
    // Attempt to use the URL as name.
    nsCOMPtr<nsIPrincipal> principal = nsContentUtils::ObjectPrincipal(global);
    if (!principal) {
      break;
    }

    nsCOMPtr<nsIURI> uri;
    nsresult rv = principal->GetURI(getter_AddRefs(uri));
    if (NS_FAILED(rv) || !uri) {
      break;
    }

    rv = uri->GetSpec(cname);
    if (NS_FAILED(rv)) {
      break;
    }

    name.Assign(NS_ConvertUTF8toUTF16(cname));
    return;
  } while(false);
  xpc::GetCurrentCompartmentName(cx, cname);
  name.Assign(NS_ConvertUTF8toUTF16(cname));
}

already_AddRefed<nsIPerformanceStats>
nsPerformanceSnapshot::ImportStats(JSContext* cx, const js::PerformanceData& performance, const uint64_t uid, nsIPerformanceStats* parent) {
  if (performance.ticks == 0) {
    // Ignore compartments with no activity.
    return nullptr;
  }
  JS::RootedObject global(cx, JS::CurrentGlobalOrNull(cx));

  if (!global) {
    // While it is possible for a compartment to have no global
    // (e.g. atoms), this compartment is not very interesting for us.
    return nullptr;
  }

  nsString groupId;
  GetGroupId(cx, uid, groupId);

  nsString addonId;
  GetAddonId(cx, global, addonId);

  nsString title;
  uint64_t windowId;
  GetWindowData(cx, title, &windowId);

  nsString name;
  GetName(cx, global, name);

  bool isSystem = GetIsSystem(cx, global);

  nsCOMPtr<nsIPerformanceStats> result =
    new nsPerformanceStats(name, parent, groupId, addonId, title, windowId, mProcessId, isSystem, performance);
  return result.forget();
}

/*static*/ bool
nsPerformanceSnapshot::IterPerformanceStatsCallback(JSContext* cx,
                                                    const js::PerformanceData& stats, const uint64_t id,
                                                    const uint64_t* parentId,
                                                    void* self) {
  return reinterpret_cast<nsPerformanceSnapshot*>(self)->IterPerformanceStatsCallbackInternal(cx, stats, id, parentId);
}

bool
nsPerformanceSnapshot::IterPerformanceStatsCallbackInternal(JSContext* cx,
                                                            const js::PerformanceData& stats, const uint64_t id,
                                                            const uint64_t* parentId) {

  nsCOMPtr<nsIPerformanceStats> parent = parentId ? mCachedStats.Get(*parentId) : nullptr;
  nsCOMPtr<nsIPerformanceStats> result = ImportStats(cx, stats, id, parent);
  if (result) {
    mComponentsData.AppendElement(result);
    mCachedStats.Put(id, result);
  }

  return true;
}

nsresult
nsPerformanceSnapshot::Init(JSContext* cx, uint64_t processId) {
  mProcessId = processId;
  js::PerformanceData processStats;
  if (!js::IterPerformanceStats(cx, nsPerformanceSnapshot::IterPerformanceStatsCallback, &processStats, this)) {
    return NS_ERROR_UNEXPECTED;
  }

  nsAutoString processGroupId;
  processGroupId.AssignLiteral("process: ");
  processGroupId.AppendInt(processId);
  mProcessData = new nsPerformanceStats(NS_LITERAL_STRING("<process>"), // name
                                        nullptr,                        // parent
                                        processGroupId,                 // group id
                                        NS_LITERAL_STRING(""),          // add-on id
                                        NS_LITERAL_STRING(""),          // title
                                        0,                              // window id
                                        mProcessId,                     // process id
                                        true,                           // isSystem
                                        processStats);
  return NS_OK;
}


NS_IMETHODIMP nsPerformanceSnapshot::GetComponentsData(nsIArray * *aComponents)
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

NS_IMETHODIMP nsPerformanceSnapshot::GetProcessData(nsIPerformanceStats * *aProcess)
{
  NS_IF_ADDREF(*aProcess = mProcessData);
  return NS_OK;
}


NS_IMPL_ISUPPORTS(nsPerformanceStatsService, nsIPerformanceStatsService, nsIObserver)

nsPerformanceStatsService::nsPerformanceStatsService()
#if defined(XP_WIN)
  : mProcessId(GetCurrentProcessId())
#else
  : mProcessId(getpid())
#endif
  , mProcessStayed(0)
  , mProcessMoved(0)
{
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    mozilla::unused << obs->AddObserver(this, "profile-before-change", false);
  }
}

nsPerformanceStatsService::~nsPerformanceStatsService()
{
}

//[implicit_jscontext] attribute bool isMonitoringCPOW;
NS_IMETHODIMP nsPerformanceStatsService::GetIsMonitoringCPOW(JSContext* cx, bool *aIsStopwatchActive)
{
  JSRuntime *runtime = JS_GetRuntime(cx);
  *aIsStopwatchActive = js::GetStopwatchIsMonitoringCPOW(runtime);
  return NS_OK;
}
NS_IMETHODIMP nsPerformanceStatsService::SetIsMonitoringCPOW(JSContext* cx, bool aIsStopwatchActive)
{
  JSRuntime *runtime = JS_GetRuntime(cx);
  if (!js::SetStopwatchIsMonitoringCPOW(runtime, aIsStopwatchActive)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}
NS_IMETHODIMP nsPerformanceStatsService::GetIsMonitoringJank(JSContext* cx, bool *aIsStopwatchActive)
{
  JSRuntime *runtime = JS_GetRuntime(cx);
  *aIsStopwatchActive = js::GetStopwatchIsMonitoringJank(runtime);
  return NS_OK;
}
NS_IMETHODIMP nsPerformanceStatsService::SetIsMonitoringJank(JSContext* cx, bool aIsStopwatchActive)
{
  JSRuntime *runtime = JS_GetRuntime(cx);
  if (!js::SetStopwatchIsMonitoringJank(runtime, aIsStopwatchActive)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}
NS_IMETHODIMP nsPerformanceStatsService::GetIsMonitoringPerCompartment(JSContext* cx, bool *aIsStopwatchActive)
{
  JSRuntime *runtime = JS_GetRuntime(cx);
  *aIsStopwatchActive = js::GetStopwatchIsMonitoringPerCompartment(runtime);
  return NS_OK;
}
NS_IMETHODIMP nsPerformanceStatsService::SetIsMonitoringPerCompartment(JSContext* cx, bool aIsStopwatchActive)
{
  JSRuntime *runtime = JS_GetRuntime(cx);
  if (!js::SetStopwatchIsMonitoringPerCompartment(runtime, aIsStopwatchActive)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

NS_IMETHODIMP nsPerformanceStatsService::GetSnapshot(JSContext* cx, nsIPerformanceSnapshot * *aSnapshot)
{
  nsRefPtr<nsPerformanceSnapshot> snapshot = new nsPerformanceSnapshot();
  nsresult rv = snapshot->Init(cx, mProcessId);
  if (NS_FAILED(rv)) {
    return rv;
  }

  js::GetPerfMonitoringTestCpuRescheduling(JS_GetRuntime(cx), &mProcessStayed, &mProcessMoved);
  snapshot.forget(aSnapshot);

  return NS_OK;
}


/* void observe (in nsISupports aSubject, in string aTopic, in wstring aData); */
NS_IMETHODIMP nsPerformanceStatsService::Observe(nsISupports *, const char *, const char16_t *)
{
  // Upload telemetry
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    mozilla::unused << obs->RemoveObserver(this, "profile-before-change");
  }

  if (mProcessStayed + mProcessMoved == 0) {
    // Nothing to report.
    return NS_OK;
  }
  const uint32_t proportion = ( 100 * mProcessStayed ) / ( mProcessStayed + mProcessMoved );
  mozilla::Telemetry::Accumulate("PERF_MONITORING_TEST_CPU_RESCHEDULING_PROPORTION_MOVED", proportion);

  return NS_OK;
}
