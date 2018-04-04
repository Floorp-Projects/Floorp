/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIMutableArray.h"
#include "nsPerformanceMetrics.h"
#include "nsThreadUtils.h"
#include "mozilla/PerformanceUtils.h"
#include "mozilla/dom/DocGroup.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/dom/WorkerDebugger.h"
#include "mozilla/dom/WorkerDebuggerManager.h"

using namespace mozilla;
using namespace mozilla::dom;

namespace mozilla {

void
CollectPerformanceInfo(nsTArray<PerformanceInfo>& aMetrics)
{
  // collecting ReportPerformanceInfo from all DocGroup instances
  for (const auto& tabChild : TabChild::GetAll()) {
    TabGroup* tabGroup = tabChild->TabGroup();
    for (auto iter = tabGroup->Iter(); !iter.Done(); iter.Next()) {
      DocGroup* docGroup = iter.Get()->mDocGroup;
      aMetrics.AppendElement(docGroup->ReportPerformanceInfo());
    }
  }

  // collecting ReportPerformanceInfo from all WorkerDebugger instances
  RefPtr<mozilla::dom::WorkerDebuggerManager> wdm = WorkerDebuggerManager::GetOrCreate();
  if (NS_WARN_IF(!wdm)) {
    return;
  }
  for (uint32_t i = 0; i < wdm->GetDebuggersLength(); i++) {
    WorkerDebugger* debugger = wdm->GetDebuggerAt(i);
    aMetrics.AppendElement(debugger->ReportPerformanceInfo());
  }
}

nsresult
NotifyPerformanceInfo(const nsTArray<PerformanceInfo>& aMetrics)
{
  nsresult rv;

  nsCOMPtr<nsIMutableArray> array = do_CreateInstance(NS_ARRAY_CONTRACTID);
  if (NS_WARN_IF(!array)) {
    return NS_ERROR_FAILURE;
  }

  // Each PerformanceInfo is converted into a nsIPerformanceMetricsData
  for (const PerformanceInfo& info : aMetrics) {
    nsCOMPtr<nsIMutableArray> items = do_CreateInstance(NS_ARRAY_CONTRACTID);
    if (NS_WARN_IF(!items)) {
      return rv;
    }
    for (const CategoryDispatch& entry : info.items()) {
      nsCOMPtr<nsIPerformanceMetricsDispatchCategory> item =
        new PerformanceMetricsDispatchCategory(entry.category(),
                                               entry.count());
      rv = items->AppendElement(item);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
    nsCOMPtr<nsIPerformanceMetricsData> data;
    data = new PerformanceMetricsData(info.pid(), info.wid(), info.pwid(),
                                      info.host(), info.duration(),
                                      info.worker(), items);
    rv = array->AppendElement(data);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (NS_WARN_IF(!obs)) {
    return NS_ERROR_FAILURE;
  }

  rv = obs->NotifyObservers(array, "performance-metrics", nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

} // namespace
