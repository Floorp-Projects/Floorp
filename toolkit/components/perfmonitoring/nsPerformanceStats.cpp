/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi.h"
#include "nsPerformanceStats.h"
#include "nsMemory.h"
#include "nsLiteralString.h"
#include "nsCRTGlue.h"
#include "nsIJSRuntimeService.h"
#include "nsServiceManagerUtils.h"
#include "nsCOMArray.h"
#include "nsIMutableArray.h"
#include "nsJSUtils.h"
#include "xpcpublic.h"
#include "jspubtd.h"

class nsPerformanceStats: public nsIPerformanceStats {
public:
  nsPerformanceStats(nsAString& aName, nsAString& aAddonId, bool aIsSystem, js::PerformanceData& aPerformanceData)
    : mName(aName)
    , mAddonId(aAddonId)
    , mIsSystem(aIsSystem)
    , mPerformanceData(aPerformanceData)
  {
  }
  explicit nsPerformanceStats() {}

  NS_DECL_ISUPPORTS

  /* readonly attribute AString name; */
  NS_IMETHOD GetName(nsAString& aName) override {
    aName.Assign(mName);
    return NS_OK;
  };

  /* readonly attribute AString addon id; */
  NS_IMETHOD GetAddonId(nsAString& aAddonId) override {
    aAddonId.Assign(mAddonId);
    return NS_OK;
  };

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
  NS_IMETHODIMP GetDurations(uint32_t *aCount, uint64_t **aNumberOfOccurrences) override {
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

  /* readonly attribute bool isSystem; */
  NS_IMETHOD GetIsSystem(bool *_retval) override {
    *_retval = mIsSystem;
    return NS_OK;
  }

private:
  nsString mName;
  nsString mAddonId;
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
  nsresult Init();
private:
  virtual ~nsPerformanceSnapshot();

  /**
   * Import a `PerformanceStats` as a `nsIPerformanceStats`.
   */
  already_AddRefed<nsIPerformanceStats> ImportStats(js::PerformanceStats* c);

  nsCOMArray<nsIPerformanceStats> mComponentsData;
  nsCOMPtr<nsIPerformanceStats> mProcessData;
};

NS_IMPL_ISUPPORTS(nsPerformanceSnapshot, nsIPerformanceSnapshot)

nsPerformanceSnapshot::nsPerformanceSnapshot()
{
}

nsPerformanceSnapshot::~nsPerformanceSnapshot()
{
}

already_AddRefed<nsIPerformanceStats>
nsPerformanceSnapshot::ImportStats(js::PerformanceStats* c) {
  nsString addonId;
  if (c->addonId) {
    AssignJSFlatString(addonId, (JSFlatString*)c->addonId);
  }
  nsCString cname(c->name);
  NS_ConvertUTF8toUTF16 name(cname);
  nsCOMPtr<nsIPerformanceStats> result = new nsPerformanceStats(name, addonId, c->isSystem, c->performance);
  return result.forget();
}

nsresult
nsPerformanceSnapshot::Init() {
  JSRuntime* rt;
  nsCOMPtr<nsIJSRuntimeService> svc(do_GetService("@mozilla.org/js/xpc/RuntimeService;1"));
  NS_ENSURE_TRUE(svc, NS_ERROR_FAILURE);
  svc->GetRuntime(&rt);
  js::PerformanceStats processStats;
  js::PerformanceStatsVector componentsStats;
  if (!js::GetPerformanceStats(rt, componentsStats, processStats)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  size_t num = componentsStats.length();
  for (size_t pos = 0; pos < num; pos++) {
    nsCOMPtr<nsIPerformanceStats> stats = ImportStats(&componentsStats[pos]);
    mComponentsData.AppendObject(stats);
  }
  mProcessData = ImportStats(&processStats);
  return NS_OK;
}


/* void getComponentsData (out nsIArray aComponents); */
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

/* readonly attribute nsIPerformanceStats process; */
NS_IMETHODIMP nsPerformanceSnapshot::GetProcessData(nsIPerformanceStats * *aProcess)
{
  NS_IF_ADDREF(*aProcess = mProcessData);
  return NS_OK;
}


NS_IMPL_ISUPPORTS(nsPerformanceStatsService, nsIPerformanceStatsService)

nsPerformanceStatsService::nsPerformanceStatsService()
{
}

nsPerformanceStatsService::~nsPerformanceStatsService()
{
}

/* [implicit_jscontext] attribute bool isStopwatchActive; */
NS_IMETHODIMP nsPerformanceStatsService::GetIsStopwatchActive(JSContext* cx, bool *aIsStopwatchActive)
{
  JSRuntime *runtime = JS_GetRuntime(cx);
  *aIsStopwatchActive = js::IsStopwatchActive(runtime);
  return NS_OK;
}
NS_IMETHODIMP nsPerformanceStatsService::SetIsStopwatchActive(JSContext* cx, bool aIsStopwatchActive)
{
  JSRuntime *runtime = JS_GetRuntime(cx);
  if (!js::SetStopwatchActive(runtime, aIsStopwatchActive)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

/* readonly attribute nsIPerformanceSnapshot snapshot; */
NS_IMETHODIMP nsPerformanceStatsService::GetSnapshot(nsIPerformanceSnapshot * *aSnapshot)
{
  nsRefPtr<nsPerformanceSnapshot> snapshot = new nsPerformanceSnapshot();
  nsresult rv = snapshot->Init();
  if (NS_FAILED(rv)) {
    return rv;
  }

  snapshot.forget(aSnapshot);
  return NS_OK;
}


