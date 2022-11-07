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

  using WindowSet = mozilla::HashSet<nsGlobalWindowOuter*>;
  WindowSet windowsVisited;
  for (const auto& document : *aDocGroup) {
    nsGlobalWindowOuter* window =
        document ? nsGlobalWindowOuter::Cast(document->GetWindow()) : nullptr;
    if (window) {
      WindowSet::AddPtr p = windowsVisited.lookupForAdd(window);
      if (!p) {
        // We have not seen this window before.
        AddWindowTabSizes(window, &sizes);
        if (!windowsVisited.add(p, window)) {
          // OOM.  Let us stop counting memory, we may undercount.
          break;
        }
      }
    }
  }

  using ZoneSet = mozilla::HashSet<JS::Zone*>;
  using SharedSet = mozilla::HashSet<void*>;
  ZoneSet zonesVisited;
  SharedSet sharedVisited;
  // Getting JS-related memory usage
  uint64_t jsMemUsed = 0;
  for (auto* doc : *aDocGroup) {
    bool unused;
    nsIGlobalObject* globalObject = doc->GetScriptHandlingObject(unused);
    if (globalObject) {
      JSObject* object = globalObject->GetGlobalJSObject();
      if (object != nullptr) {
        MOZ_ASSERT(NS_IsMainThread(),
                   "We cannot get the object zone on another thread");
        JS::Zone* zone = JS::GetObjectZone(object);
        ZoneSet::AddPtr addZone = zonesVisited.lookupForAdd(zone);
        if (!addZone) {
          // We have not checked this zone before.
          jsMemUsed += js::GetMemoryUsageForZone(zone);
          if (!zonesVisited.add(addZone, zone)) {
            // OOM.  Let us stop counting memory, we may undercount.
            break;
          }

          const js::gc::SharedMemoryMap& shared =
              js::GetSharedMemoryUsageForZone(zone);
          for (auto iter = shared.iter(); !iter.done(); iter.next()) {
            void* sharedMem = iter.get().key();
            SharedSet::AddPtr addShared = sharedVisited.lookupForAdd(sharedMem);
            if (addShared) {
              // We *have* seen this shared memory before.

              // Because shared memory is already included in
              // js::GetMemoryUsageForZone() above, and we've seen it for a
              // previous zone, we subtract it here so it's not counted more
              // than once.
              jsMemUsed -= iter.get().value().nbytes;
            } else if (!sharedVisited.add(addShared, sharedMem)) {
              // As above, abort with an under-estimate.
              break;
            }
          }
        }
      }
    }
  }

  // Getting Media sizes.
  return GetMediaMemorySizes()->Then(
      aEventTarget, __func__,
      [jsMemUsed, sizes](const MediaMemoryInfo& media) {
        return MemoryPromise::CreateAndResolve(
            PerformanceMemoryInfo(media, sizes.mDom, sizes.mStyle, sizes.mOther,
                                  jsMemUsed),
            __func__);
      },
      [](const nsresult rv) {
        return MemoryPromise::CreateAndReject(rv, __func__);
      });
}

}  // namespace mozilla
