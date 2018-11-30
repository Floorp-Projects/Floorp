/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsThreadUtils.h"
#include "mozilla/PerformanceUtils.h"
#include "mozilla/dom/DocGroup.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/dom/WorkerDebugger.h"
#include "mozilla/dom/WorkerDebuggerManager.h"

#include "MediaDecoder.h"
#include "XPCJSMemoryReporter.h"
#include "jsfriendapi.h"
#include "js/MemoryMetrics.h"
#include "nsWindowMemoryReporter.h"
#include "nsDOMWindowList.h"

using namespace mozilla;
using namespace mozilla::dom;

namespace mozilla {

nsTArray<RefPtr<PerformanceInfoPromise>> CollectPerformanceInfo() {
  nsTArray<RefPtr<PerformanceInfoPromise>> promises;

  // collecting ReportPerformanceInfo from all WorkerDebugger instances
  RefPtr<mozilla::dom::WorkerDebuggerManager> wdm =
      WorkerDebuggerManager::GetOrCreate();
  if (NS_WARN_IF(!wdm)) {
    return promises;
  }

  for (uint32_t i = 0; i < wdm->GetDebuggersLength(); i++) {
    const RefPtr<WorkerDebugger> debugger = wdm->GetDebuggerAt(i);
    promises.AppendElement(debugger->ReportPerformanceInfo());
  }

  // collecting ReportPerformanceInfo from all DocGroup instances
  LinkedList<TabGroup>* tabGroups = TabGroup::GetTabGroupList();

  // if GetTabGroupList() returns null, we don't have any tab group
  if (tabGroups) {
    for (TabGroup* tabGroup = tabGroups->getFirst(); tabGroup;
         tabGroup =
             static_cast<LinkedListElement<TabGroup>*>(tabGroup)->getNext()) {
      for (auto iter = tabGroup->Iter(); !iter.Done(); iter.Next()) {
        DocGroup* docGroup = iter.Get()->mDocGroup;
        promises.AppendElement(docGroup->ReportPerformanceInfo());
      }
    }
  }
  return promises;
}

nsresult GetTabSizes(nsGlobalWindowOuter* aWindow, nsTabSizes* aSizes) {
  // Measure the window.
  SizeOfState state(moz_malloc_size_of);
  nsWindowSizes windowSizes(state);
  aWindow->AddSizeOfIncludingThis(windowSizes);

  // Measure the inner window, if there is one.
  nsGlobalWindowInner* inner = aWindow->GetCurrentInnerWindowInternal();
  if (inner != nullptr) {
    inner->AddSizeOfIncludingThis(windowSizes);
  }

  windowSizes.addToTabSizes(aSizes);
  nsDOMWindowList* frames = aWindow->GetFrames();
  uint32_t length = frames->GetLength();

  // Measure this window's descendents.
  for (uint32_t i = 0; i < length; i++) {
    nsCOMPtr<nsPIDOMWindowOuter> child = frames->IndexedGetter(i);
    NS_ENSURE_STATE(child);
    nsGlobalWindowOuter* childWin = nsGlobalWindowOuter::Cast(child);
    nsresult rv = GetTabSizes(childWin, aSizes);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

RefPtr<MemoryPromise> CollectMemoryInfo(
    const nsCOMPtr<nsPIDOMWindowOuter>& aWindow,
    const RefPtr<AbstractThread>& aEventTarget) {
  // Getting Dom sizes. -- XXX should we reimplement GetTabSizes to async here ?
  nsGlobalWindowOuter* window = nsGlobalWindowOuter::Cast(aWindow);
  nsTabSizes sizes;
  nsresult rv = GetTabSizes(window, &sizes);
  if (NS_FAILED(rv)) {
    return MemoryPromise::CreateAndReject(rv, __func__);
  }

  // Getting GC Heap Usage
  JSObject* obj = window->GetGlobalJSObject();
  uint64_t GCHeapUsage = 0;
  if (obj != nullptr) {
    GCHeapUsage = js::GetGCHeapUsageForObjectZone(obj);
  }

  // Getting Media sizes.
  return GetMediaMemorySizes()->Then(
      aEventTarget, __func__,
      [GCHeapUsage, sizes](const MediaMemoryInfo& media) {
        return MemoryPromise::CreateAndResolve(
            PerformanceMemoryInfo(media, sizes.mDom, sizes.mStyle, sizes.mOther,
                                  GCHeapUsage),
            __func__);
      },
      [](const nsresult rv) {
        return MemoryPromise::CreateAndReject(rv, __func__);
      });
}

}  // namespace mozilla
