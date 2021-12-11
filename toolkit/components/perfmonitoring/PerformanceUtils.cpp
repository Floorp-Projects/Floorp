/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/PerformanceUtils.h"

#include "mozilla/dom/DOMTypes.h"
#include "mozilla/dom/DocGroup.h"
#include "mozilla/dom/BrowsingContextGroup.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/WorkerDebugger.h"
#include "mozilla/dom/WorkerDebuggerManager.h"

#include "MediaDecoder.h"
#include "jsfriendapi.h"
#include "nsGlobalWindowOuter.h"
#include "nsWindowSizes.h"

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

  nsTArray<RefPtr<BrowsingContextGroup>> groups;
  BrowsingContextGroup::GetAllGroups(groups);

  nsTArray<DocGroup*> docGroups;
  for (auto& browsingContextGroup : groups) {
    browsingContextGroup->GetDocGroups(docGroups);
  }

  for (DocGroup* docGroup : docGroups) {
    promises.AppendElement(docGroup->ReportPerformanceInfo());
  }

  return promises;
}

void AddWindowTabSizes(nsGlobalWindowOuter* aWindow, nsTabSizes* aSizes) {
  Document* document = aWindow->GetDocument();
  if (document && document->GetCachedSizes(aSizes)) {
    // We got a cached version
    return;
  }
  // We measure the sizes on a fresh nsTabSizes instance
  // because we want to cache the value and aSizes might
  // already have some values from other windows.
  nsTabSizes sizes;

  // Measure the window.
  SizeOfState state(moz_malloc_size_of);
  nsWindowSizes windowSizes(state);
  aWindow->AddSizeOfIncludingThis(windowSizes);
  // Measure the inner window, if there is one.
  nsGlobalWindowInner* inner = aWindow->GetCurrentInnerWindowInternal();
  if (inner != nullptr) {
    inner->AddSizeOfIncludingThis(windowSizes);
  }
  windowSizes.addToTabSizes(&sizes);
  if (document) {
    document->SetCachedSizes(&sizes);
  }
  aSizes->mDom += sizes.mDom;
  aSizes->mStyle += sizes.mStyle;
  aSizes->mOther += sizes.mOther;
}

nsresult GetTabSizes(BrowsingContext* aContext, nsTabSizes* aSizes) {
  if (!aContext) {
    return NS_OK;
  }

  // Add the window (and inner window) sizes. Might be cached.
  nsGlobalWindowOuter* window =
      nsGlobalWindowOuter::Cast(aContext->GetDOMWindow());
  if (window) {
    AddWindowTabSizes(window, aSizes);
  }

  // Measure this window's descendents.
  for (const auto& child : aContext->Children()) {
    MOZ_TRY(GetTabSizes(child, aSizes));
  }
  return NS_OK;
}

RefPtr<MemoryPromise> CollectMemoryInfo(
    const RefPtr<DocGroup>& aDocGroup,
    const RefPtr<AbstractThread>& aEventTarget) {
  // Getting Dom sizes. -- XXX should we reimplement GetTabSizes to async here ?
  nsTabSizes sizes;

  for (const auto& document : *aDocGroup) {
    nsGlobalWindowOuter* window =
        document ? nsGlobalWindowOuter::Cast(document->GetWindow()) : nullptr;
    if (window) {
      AddWindowTabSizes(window, &sizes);
    }
  }

  BrowsingContextGroup* group = aDocGroup->GetBrowsingContextGroup();
  // Getting GC Heap Usage
  uint64_t GCHeapUsage = 0;
  JSObject* object = group->GetWrapper();
  if (object != nullptr) {
    GCHeapUsage = js::GetGCHeapUsageForObjectZone(object);
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

RefPtr<MemoryPromise> CollectMemoryInfo(
    const RefPtr<BrowsingContext>& aContext,
    const RefPtr<AbstractThread>& aEventTarget) {
  // Getting Dom sizes. -- XXX should we reimplement GetTabSizes to async here ?
  nsTabSizes sizes;
  nsresult rv = GetTabSizes(aContext, &sizes);
  if (NS_FAILED(rv)) {
    return MemoryPromise::CreateAndReject(rv, __func__);
  }

  // Getting GC Heap Usage
  JSObject* obj = aContext->GetWrapper();
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
